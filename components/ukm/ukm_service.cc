// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/scheme_constants.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "components/ukm/ukm_rotation_scheduler.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_recorder_client_interface_registry.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/ukm/web_features.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace ukm {

namespace {

// Generates a new client id and stores it in prefs.
uint64_t GenerateAndStoreClientId(PrefService* pref_service) {
  uint64_t client_id = 0;
  while (!client_id) {
    client_id = base::RandUint64();
  }
  pref_service->SetUint64(prefs::kUkmClientId, client_id);

  // Also reset the session id counter.
  pref_service->SetInteger(prefs::kUkmSessionId, 0);
  return client_id;
}

uint64_t LoadOrGenerateAndStoreClientId(PrefService* pref_service,
                                        uint64_t external_client_id) {
  // If external_client_id is present, save to pref service for
  // consistency purpose and return it as client id.
  if (external_client_id) {
    pref_service->SetUint64(prefs::kUkmClientId, external_client_id);
    return external_client_id;
  }

  uint64_t client_id = pref_service->GetUint64(prefs::kUkmClientId);
  // The pref is stored as a string and GetUint64() uses base::StringToUint64()
  // to convert it. base::StringToUint64() will treat a negative value as
  // underflow, which results in 0 (the minimum Uint64 value).
  if (client_id) {
    return client_id;
  }

  // Since client_id was 0, the pref value may have been negative. Attempt to
  // get it as an Int64 to migrate it to Uint64.
  client_id = pref_service->GetInt64(prefs::kUkmClientId);
  if (client_id) {
    pref_service->SetUint64(prefs::kUkmClientId, client_id);
    return client_id;
  }

  // The client_id is still 0, so it wasn't set.
  return GenerateAndStoreClientId(pref_service);
}

int32_t LoadAndIncrementSessionId(PrefService* pref_service) {
  int32_t session_id = pref_service->GetInteger(prefs::kUkmSessionId);
  ++session_id;  // Increment session id, once per session.
  pref_service->SetInteger(prefs::kUkmSessionId, session_id);
  return session_id;
}

metrics::UkmLogSourceType GetLogSourceTypeFromSources(
    const google::protobuf::RepeatedPtrField<Source>& sources) {
  bool contains_appkm = false;
  bool contains_ukm = false;
  for (Source source : sources) {
    if (source.type() == SourceType::APP_ID) {
      contains_appkm = true;
    } else {
      contains_ukm = true;
    }
  }
  if (contains_appkm && contains_ukm) {
    return metrics::UkmLogSourceType::BOTH_UKM_AND_APPKM;
  } else if (contains_appkm) {
    return metrics::UkmLogSourceType::APPKM_ONLY;
  } else {
    return metrics::UkmLogSourceType::UKM_ONLY;
  }
}

// Remove elements satisfying the predicate by moving them to the end of the
// list then truncate.
template <typename Predicate, typename ReadElements, typename WriteElements>
void FilterReportElements(Predicate predicate,
                          const ReadElements& elements,
                          WriteElements* mutable_elements) {
  if (elements.empty()) {
    return;
  }

  int entries_size = elements.size();
  int start = 0;
  int end = entries_size - 1;
  // This loop ensures that everything to the left of start doesn't satisfy the
  // predicate and everything to the right of end does. If start == end then we
  // don't know if whether or predicate(elements.Get(start)) is true so the
  // condition needs to be <=.
  while (start <= end) {
    while (start < entries_size && !predicate(elements.Get(start))) {
      start++;
    }
    while (end >= 0 && predicate(elements.Get(end))) {
      end--;
    }
    if (start < end) {
      mutable_elements->SwapElements(start, end);
      // Thanks to the swap predicate(elements.Get(start)) is now false and
      // predicate(elements.Get(end)) is now true so it's safe unconditionally
      // increment and decrement start and end respectively.
      start++;
      end--;
    }
  }
  mutable_elements->DeleteSubrange(start, entries_size - start);
}

template <typename Predicate>
void PurgeDataFromUnsentLogStore(metrics::UnsentLogStore* ukm_log_store,
                                 Predicate source_purging_condition,
                                 const std::string& current_version) {
  for (size_t index = 0; index < ukm_log_store->size(); index++) {
    // Decode log data from store back into a Report.
    Report report;
    bool decode_success = metrics::DecodeLogDataToProto(
        ukm_log_store->GetLogAtIndex(index), &report);
    DCHECK(decode_success);

    std::unordered_set<SourceId> relevant_source_ids;

    // Grab ids of all sources satisfying the condition for purging.
    for (const auto& source : report.sources()) {
      if (source_purging_condition(source)) {
        relevant_source_ids.insert(source.id());
      }
    }
    if (relevant_source_ids.empty()) {
      continue;
    }

    // Remove all relevant sources from the report.
    FilterReportElements(
        [&](const Source& element) {
          return relevant_source_ids.count(element.id());
        },
        report.sources(), report.mutable_sources());

    // Remove all entries originating from these sources.
    FilterReportElements(
        [&](const Entry& element) {
          return relevant_source_ids.count(element.source_id());
        },
        report.entries(), report.mutable_entries());

    // Remove all web features data originating from these sources.
    FilterReportElements(
        [&](const HighLevelWebFeatures& element) {
          return relevant_source_ids.count(element.source_id());
        },
        report.web_features(), report.mutable_web_features());

    const bool app_version_changed =
        report.system_profile().app_version() != current_version;
    UMA_HISTOGRAM_BOOLEAN("UKM.AppVersionDifferentWhenPurging",
                          app_version_changed);
    if (app_version_changed) {
      report.mutable_system_profile()->set_log_written_by_app_version(
          current_version);
    }
    std::string reserialized_log_data =
        UkmService::SerializeReportProtoToString(&report);

    // Replace the compressed log in the store by its filtered version.
    metrics::LogMetadata log_metadata;
    log_metadata.log_source_type =
        GetLogSourceTypeFromSources(report.sources());

    const std::string old_compressed_log_data =
        ukm_log_store->ReplaceLogAtIndex(index, reserialized_log_data,
                                         log_metadata);

    // Reached here only if some Sources satisfied the condition for purging, so
    // reserialized data should now be different.
    DCHECK(ukm_log_store->GetLogAtIndex(index) != old_compressed_log_data);
  }
}

}  // namespace

// static
BASE_FEATURE(kReportUserNoisedUserBirthYearAndGender,
             "UkmReportNoisedUserBirthYearAndGender",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool UkmService::LogCanBeParsed(const std::string& serialized_data) {
  Report report;
  bool report_parse_successful = report.ParseFromString(serialized_data);
  if (!report_parse_successful) {
    return false;
  }
  // Make sure the reserialized log from this |report| matches the input
  // |serialized_data|.
  std::string reserialized_from_report;
  report.SerializeToString(&reserialized_from_report);
  return reserialized_from_report == serialized_data;
}

std::string UkmService::SerializeReportProtoToString(Report* report) {
  std::string serialized_full_log;
  report->SerializeToString(&serialized_full_log);

  // This allows catching errors with bad UKM serialization we've seen before
  // that would otherwise only be noticed on the server.
  DCHECK(UkmService::LogCanBeParsed(serialized_full_log));
  return serialized_full_log;
}

UkmService::UkmService(PrefService* pref_service,
                       metrics::MetricsServiceClient* client,
                       std::unique_ptr<metrics::UkmDemographicMetricsProvider>
                           demographics_provider,
                       uint64_t external_client_id)
    : recorder_client_registry_(
          std::make_unique<metrics::UkmRecorderClientInterfaceRegistry>()),
      pref_service_(pref_service),
      external_client_id_(external_client_id),
      client_(client),
      demographics_provider_(std::move(demographics_provider)),
      reporting_service_(client, pref_service),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(pref_service_);
  DCHECK(client_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::Constructor";
  reporting_service_.Initialize();

  cloned_install_subscription_ = client->AddOnClonedInstallDetectedCallback(
      base::BindOnce(&UkmService::OnClonedInstallDetected,
                     self_ptr_factory_.GetWeakPtr()));

  base::RepeatingClosure rotate_callback = base::BindRepeating(
      &UkmService::RotateLog, self_ptr_factory_.GetWeakPtr());
  // MetricsServiceClient outlives UkmService, and
  // MetricsReportingScheduler is tied to the lifetime of |this|.
  const base::RepeatingCallback<base::TimeDelta(void)>&
      get_upload_interval_callback =
          base::BindRepeating(&metrics::MetricsServiceClient::GetUploadInterval,
                              base::Unretained(client_));
  bool fast_startup_for_testing = client_->ShouldStartUpFastForTesting();
  scheduler_ = std::make_unique<UkmRotationScheduler>(
      rotate_callback, fast_startup_for_testing, get_upload_interval_callback);
  InitDecodeMap();

  DelegatingUkmRecorder::Get()->AddDelegate(self_ptr_factory_.GetWeakPtr());
}

UkmService::~UkmService() {
  UkmRecorder::Get()->NotifyStartShutdown();
  DisableReporting();
  DelegatingUkmRecorder::Get()->RemoveDelegate(this);
}

void UkmService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialize_started_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::Initialize";
  initialize_started_ = true;

  DCHECK_EQ(0, report_count_);
  if (client_->ShouldResetClientIdsOnClonedInstall()) {
    ResetClientState(ResetReason::kClonedInstall);
  } else {
    client_id_ =
        LoadOrGenerateAndStoreClientId(pref_service_, external_client_id_);
    session_id_ = LoadAndIncrementSessionId(pref_service_);
  }

  metrics_providers_.Init();

  StartInitTask();
}

void UkmService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::EnableReporting";
  if (reporting_service_.reporting_active()) {
    return;
  }

  log_creation_time_ = base::TimeTicks::Now();
  metrics_providers_.OnRecordingEnabled();

  if (!initialize_started_) {
    Initialize();
  }
  scheduler_->Start();
  reporting_service_.EnableReporting();
}

void UkmService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::DisableReporting";

  reporting_service_.DisableReporting();

  metrics_providers_.OnRecordingDisabled();

  scheduler_->Stop();
  Flush(metrics::MetricsLogsEventManager::CreateReason::kServiceShutdown);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void UkmService::OnAppEnterForeground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Medium) << "UkmService::OnAppEnterForeground";

  reporting_service_.SetIsInForegound(true);

  // If initialize_started_ is false, UKM has not yet been started, so bail. The
  // scheduler will instead be started via EnableReporting().
  if (!initialize_started_) {
    return;
  }

  scheduler_->Start();
}

void UkmService::OnAppEnterBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Medium) << "UkmService::OnAppEnterBackground";

  reporting_service_.SetIsInForegound(false);

  if (!initialize_started_) {
    return;
  }

  scheduler_->Stop();

  // Give providers a chance to persist ukm data as part of being backgrounded.
  metrics_providers_.OnAppEnterBackground();

  Flush(metrics::MetricsLogsEventManager::CreateReason::kBackgrounded);
}
#endif

void UkmService::Flush(metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialize_complete_) {
    BuildAndStoreLog(reason);
  }
  reporting_service_.ukm_log_store()->TrimAndPersistUnsentLogs(
      /*overwrite_in_memory_store=*/true);
}

void UkmService::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::Purge";
  reporting_service_.ukm_log_store()->Purge();
  UkmRecorderImpl::Purge();
}

void UkmService::PurgeExtensionsData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::PurgeExtensionsData";
  // Filter out any extension-related data from the serialized logs in the
  // UnsentLogStore for uploading, base on having kExtensionScheme URL scheme.
  PurgeDataFromUnsentLogStore(
      reporting_service_.ukm_log_store(),
      [&](const Source& source) {
        // Check if any URL on the Source has the kExtensionScheme URL scheme.
        // It is possible that only one of multiple URLs does due to redirect,
        // in this case, we should still purge the source.
        for (const auto& url_info : source.urls()) {
          if (GURL(url_info.url()).SchemeIs(kExtensionScheme)) {
            return true;
          }
        }
        return false;
      },
      client_->GetVersionString());

  // Purge data currently in the recordings intended for the next
  // ukm::Report.
  UkmRecorderImpl::PurgeRecordingsWithUrlScheme(kExtensionScheme);
}

void UkmService::PurgeAppsData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::PurgeAppsData";
  // Filter out any apps-related data from the serialized logs in the
  // UnsentLogStore for uploading.
  // Also purge based on source id type, because some apps don't use app://
  // scheme.
  // For example, OS Settings is an ChromeOS app with "chrome://os-settings" as
  // its URL.
  PurgeDataFromUnsentLogStore(
      reporting_service_.ukm_log_store(),
      [&](const Source& source) {
        if (GetSourceIdType(source.id()) == SourceIdType::APP_ID) {
          return true;
        }
        for (const auto& url_info : source.urls()) {
          if (GURL(url_info.url()).SchemeIs(kAppScheme)) {
            return true;
          }
        }
        return false;
      },
      client_->GetVersionString());

  // Purge data currently in the recordings intended for the next ukm::Report.
  UkmRecorderImpl::PurgeRecordingsWithUrlScheme(kAppScheme);
  UkmRecorderImpl::PurgeRecordingsWithSourceIdType(SourceIdType::APP_ID);
}

void UkmService::PurgeMsbbData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Filter out any MSBB-related data from the serialized logs in the
  // UnsentLogStore for uploading.
  PurgeDataFromUnsentLogStore(
      reporting_service_.ukm_log_store(),
      [&](const Source& source) {
        return UkmRecorderImpl::GetConsentType(GetSourceIdType(source.id())) ==
               MSBB;
      },
      client_->GetVersionString());

  // Purge data currently in the recordings intended for the next ukm::Report.
  UkmRecorderImpl::PurgeRecordingsWithMsbbSources();
}

void UkmService::ResetClientState(ResetReason reason) {
  DVLOG(DebuggingLogLevel::Rare)
      << "ResetClientState [reason=" << static_cast<int>(reason) << "]";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION("UKM.ResetReason", reason);

  if (external_client_id_) {
    client_id_ = external_client_id_;
    pref_service_->SetUint64(prefs::kUkmClientId, client_id_);
  } else {
    client_id_ = GenerateAndStoreClientId(pref_service_);
  }

  // Note: the session_id has already been cleared by GenerateAndStoreClientId.
  session_id_ = LoadAndIncrementSessionId(pref_service_);
  report_count_ = 0;

  metrics_providers_.OnClientStateCleared();
}

void UkmService::OnClonedInstallDetected() {
  DVLOG(DebuggingLogLevel::Rare)
      << "OnClonedInstallDetected. UKM logs will be purged.";
  // Purge all logs, as they may come from a previous install. Unfortunately,
  // since the cloned install detector works asynchronously, it is possible that
  // this is called after logs were already sent. However, practically speaking,
  // this should not happen, since logs are only sent late into the session.
  reporting_service_.ukm_log_store()->Purge();
}

void UkmService::RegisterMetricsProvider(
    std::unique_ptr<metrics::MetricsProvider> provider) {
  metrics_providers_.RegisterMetricsProvider(std::move(provider));
}

void UkmService::RegisterEventFilter(std::unique_ptr<UkmEntryFilter> filter) {
  SetEntryFilter(std::move(filter));
}

// static
void UkmService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(prefs::kUkmClientId, 0);
  registry->RegisterIntegerPref(prefs::kUkmSessionId, 0);
  UkmReportingService::RegisterPrefs(registry);
}

void UkmService::OnRecorderParametersChanged() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UkmService::OnRecorderParametersChangedImpl,
                                self_ptr_factory_.GetWeakPtr()));
}

void UkmService::OnRecorderParametersChangedImpl() {
  auto params = mojom::UkmRecorderParameters::New();
  params->is_enabled = recording_enabled();

  std::set<uint64_t> events = GetObservedEventHashes();
  params->event_hash_bypass_list.insert(params->event_hash_bypass_list.end(),
                                        events.begin(), events.end());
  recorder_client_registry_->SetRecorderParameters(std::move(params));
}

void UkmService::StartInitTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::StartInitTask";
  metrics_providers_.AsyncInit(base::BindOnce(&UkmService::FinishedInitTask,
                                              self_ptr_factory_.GetWeakPtr()));
}

void UkmService::FinishedInitTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::FinishedInitTask";
  initialize_complete_ = true;
  scheduler_->InitTaskComplete();
  if (initialization_complete_callback_) {
    std::move(initialization_complete_callback_).Run();
  }
}

void UkmService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(DebuggingLogLevel::Rare) << "UkmService::RotateLog";
  if (!reporting_service_.ukm_log_store()->has_unsent_logs()) {
    BuildAndStoreLog(metrics::MetricsLogsEventManager::CreateReason::kPeriodic);
  }
  reporting_service_.Start();
  scheduler_->RotationFinished();
}

void UkmService::AddSyncedUserNoiseBirthYearAndGenderToReport(Report* report) {
  if (!base::FeatureList::IsEnabled(kReportUserNoisedUserBirthYearAndGender) ||
      !demographics_provider_) {
    return;
  }

  demographics_provider_->ProvideSyncedUserNoisedBirthYearAndGenderToReport(
      report);
}

void UkmService::BuildAndStoreLog(
    metrics::MetricsLogsEventManager::CreateReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This may add new UKMs. This means this needs to be done before the empty
  // log suppression checks.
  metrics_providers_.ProvideCurrentSessionUKMData();

  // Suppress generating a log if we have no new data to include.
  bool empty =
      sources().empty() && entries().empty() && webdx_features().empty();
  UMA_HISTOGRAM_BOOLEAN("UKM.BuildAndStoreLogIsEmpty", empty);
  if (empty) {
    DVLOG(DebuggingLogLevel::Rare) << "No local UKM data. No log created.";
    return;
  }

  Report report;
  report.set_client_id(client_id_);
  report.set_session_id(session_id_);
  report.set_report_id(++report_count_);
  DVLOG(DebuggingLogLevel::Rare)
      << "UkmService::BuildAndStoreLog [report_id=" << report_count_ << "]";

  const auto product = static_cast<metrics::ChromeUserMetricsExtension_Product>(
      client_->GetProduct());
  // Only set the product if it differs from the default value.
  if (product != report.product()) {
    report.set_product(product);
  }

  StoreRecordingsInReport(&report);

  metrics::MetricsLog::RecordCoreSystemProfile(client_,
                                               report.mutable_system_profile());

  metrics_providers_.ProvideSystemProfileMetricsWithLogCreationTime(
      log_creation_time_, report.mutable_system_profile());

  AddSyncedUserNoiseBirthYearAndGenderToReport(&report);

  std::string serialized_log =
      UkmService::SerializeReportProtoToString(&report);

  metrics::LogMetadata log_metadata;
  log_metadata.log_source_type = GetLogSourceTypeFromSources(report.sources());

  reporting_service_.ukm_log_store()->StoreLog(serialized_log, log_metadata,
                                               reason);
}

void UkmService::SetInitializationCompleteCallbackForTesting(
    base::OnceClosure callback) {
  if (initialize_complete_) {
    std::move(callback).Run();
  } else {
    // Store the callback to be invoked when initialization is complete later.
    initialization_complete_callback_ = std::move(callback);
  }
}

}  // namespace ukm
