// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/ukm_demographic_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/scheme_constants.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_rotation_scheduler.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "third_party/metrics_proto/ukm/report.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace ukm {

namespace {

// Generates a new client id and stores it in prefs.
uint64_t GenerateAndStoreClientId(PrefService* pref_service) {
  uint64_t client_id = 0;
  while (!client_id)
    client_id = base::RandUint64();
  pref_service->SetUint64(prefs::kUkmClientId, client_id);

  // Also reset the session id counter.
  pref_service->SetInteger(prefs::kUkmSessionId, 0);
  return client_id;
}

uint64_t LoadOrGenerateAndStoreClientId(PrefService* pref_service) {
  uint64_t client_id = pref_service->GetUint64(prefs::kUkmClientId);
  // The pref is stored as a string and GetUint64() uses base::StringToUint64()
  // to convert it. base::StringToUint64() will treat a negative value as
  // underflow, which results in 0 (the minimum Uint64 value).
  if (client_id) {
    UMA_HISTOGRAM_BOOLEAN("UKM.MigratedClientIdInt64ToUInt64", false);
    return client_id;
  }

  // Since client_id was 0, the pref value may have been negative. Attempt to
  // get it as an Int64 to migrate it to Uint64.
  client_id = pref_service->GetInt64(prefs::kUkmClientId);
  if (client_id) {
    pref_service->SetUint64(prefs::kUkmClientId, client_id);
    UMA_HISTOGRAM_BOOLEAN("UKM.MigratedClientIdInt64ToUInt64", true);
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

// Remove elements satisfying the predicate by moving them to the end of the
// list then truncate.
template <typename Predicate, typename ReadElements, typename WriteElements>
void FilterReportElements(Predicate predicate,
                          const ReadElements& elements,
                          WriteElements* mutable_elements) {
  if (elements.empty())
    return;

  int entries_size = elements.size();
  int start = 0;
  int end = entries_size - 1;
  while (start < end) {
    while (start < entries_size && !predicate(elements.Get(start))) {
      start++;
    }
    while (end >= 0 && predicate(elements.Get(end))) {
      end--;
    }
    if (start < end) {
      mutable_elements->SwapElements(start, end);
      start++;
      end--;
    }
  }
  mutable_elements->DeleteSubrange(start, entries_size - start);
}

void PurgeExtensionDataFromUnsentLogStore(
    metrics::UnsentLogStore* ukm_log_store) {
  for (size_t index = 0; index < ukm_log_store->size(); index++) {
    // Uncompress log data from store back into a Report.
    const std::string& compressed_log_data =
        ukm_log_store->GetLogAtIndex(index);
    std::string uncompressed_log_data;
    const bool uncompress_successful = compression::GzipUncompress(
        compressed_log_data, &uncompressed_log_data);
    DCHECK(uncompress_successful);
    Report report;

    const bool report_parse_successful =
        report.ParseFromString(uncompressed_log_data);
    DCHECK(report_parse_successful);

    std::unordered_set<SourceId> extension_source_ids;

    // Grab all extension-related source ids.
    for (const auto& source : report.sources()) {
      // Check if any URL on the source has extension scheme. It is possible
      // that only one of multiple URLs does due to redirect, in this case, we
      // should still purge the source.
      for (const auto& url_info : source.urls()) {
        if (GURL(url_info.url()).SchemeIs(kExtensionScheme)) {
          extension_source_ids.insert(source.id());
          break;
        }
      }
    }
    if (extension_source_ids.empty())
      continue;

    // Remove all extension-related sources from the report.
    FilterReportElements(
        [&](const Source& element) {
          return extension_source_ids.count(element.id());
        },
        report.sources(), report.mutable_sources());

    // Remove all entries originating from extension-related sources.
    FilterReportElements(
        [&](const Entry& element) {
          return extension_source_ids.count(element.source_id());
        },
        report.entries(), report.mutable_entries());

    std::string reserialized_log_data;
    report.SerializeToString(&reserialized_log_data);

    // Replace the compressed log in the store by its filtered version.
    const std::string old_compressed_log_data =
        ukm_log_store->ReplaceLogAtIndex(index, reserialized_log_data);

    // Reached here only if extensions were found in the log, so data should now
    // be different after filtering.
    DCHECK(ukm_log_store->GetLogAtIndex(index) != old_compressed_log_data);
  }
}

}  // namespace

UkmService::UkmService(PrefService* pref_service,
                       metrics::MetricsServiceClient* client,
                       bool restrict_to_whitelist_entries,
                       std::unique_ptr<metrics::UkmDemographicMetricsProvider>
                           demographics_provider)
    : pref_service_(pref_service),
      restrict_to_whitelist_entries_(restrict_to_whitelist_entries),
      client_(client),
      demographics_provider_(std::move(demographics_provider)),
      reporting_service_(client, pref_service) {
  DCHECK(pref_service_);
  DCHECK(client_);
  DCHECK(demographics_provider_);
  DVLOG(1) << "UkmService::Constructor";

  reporting_service_.Initialize();

  base::RepeatingClosure rotate_callback = base::BindRepeating(
      &UkmService::RotateLog, self_ptr_factory_.GetWeakPtr());
  // MetricsServiceClient outlives UkmService, and
  // MetricsReportingScheduler is tied to the lifetime of |this|.
  const base::RepeatingCallback<base::TimeDelta(void)>&
      get_upload_interval_callback =
          base::BindRepeating(&metrics::MetricsServiceClient::GetUploadInterval,
                              base::Unretained(client_));
  bool fast_startup_for_testing = client_->ShouldStartUpFastForTesting();
  scheduler_.reset(new UkmRotationScheduler(
      rotate_callback, fast_startup_for_testing, get_upload_interval_callback));
  StoreWhitelistedEntries();

  DelegatingUkmRecorder::Get()->AddDelegate(self_ptr_factory_.GetWeakPtr());
}

UkmService::~UkmService() {
  DisableReporting();
  DelegatingUkmRecorder::Get()->RemoveDelegate(this);
}

void UkmService::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialize_started_);
  DVLOG(1) << "UkmService::Initialize";
  initialize_started_ = true;

  DCHECK_EQ(0, report_count_);
  client_id_ = LoadOrGenerateAndStoreClientId(pref_service_);
  session_id_ = LoadAndIncrementSessionId(pref_service_);
  metrics_providers_.Init();

  StartInitTask();
}

void UkmService::EnableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::EnableReporting";
  if (reporting_service_.reporting_active())
    return;

  log_creation_time_ = base::TimeTicks::Now();
  metrics_providers_.OnRecordingEnabled();

  if (!initialize_started_)
    Initialize();
  scheduler_->Start();
  reporting_service_.EnableReporting();
}

void UkmService::DisableReporting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::DisableReporting";

  reporting_service_.DisableReporting();

  metrics_providers_.OnRecordingDisabled();

  scheduler_->Stop();
  Flush();
}

#if defined(OS_ANDROID) || defined(OS_IOS)
void UkmService::OnAppEnterForeground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::OnAppEnterForeground";

  // If initialize_started_ is false, UKM has not yet been started, so bail. The
  // scheduler will instead be started via EnableReporting().
  if (!initialize_started_)
    return;

  scheduler_->Start();
}

void UkmService::OnAppEnterBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::OnAppEnterBackground";

  if (!initialize_started_)
    return;

  scheduler_->Stop();

  // Give providers a chance to persist ukm data as part of being backgrounded.
  metrics_providers_.OnAppEnterBackground();

  Flush();
}
#endif

void UkmService::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (initialize_complete_)
    BuildAndStoreLog();
  reporting_service_.ukm_log_store()->PersistUnsentLogs();
}

void UkmService::Purge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::Purge";
  reporting_service_.ukm_log_store()->Purge();
  UkmRecorderImpl::Purge();
}

void UkmService::PurgeExtensions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::PurgeExtensions";
  // Filter out any extension-related data from the serialized logs in the
  // UnsentLogStore for uploading.
  PurgeExtensionDataFromUnsentLogStore(reporting_service_.ukm_log_store());
  // Purge data currently in the recordings intended for the next ukm::Report.
  UkmRecorderImpl::PurgeExtensionRecordings();
}

void UkmService::ResetClientState(ResetReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UMA_HISTOGRAM_ENUMERATION("UKM.ResetReason", reason);

  client_id_ = GenerateAndStoreClientId(pref_service_);
  // Note: the session_id has already been cleared by GenerateAndStoreClientId.
  session_id_ = LoadAndIncrementSessionId(pref_service_);
  report_count_ = 0;
}

void UkmService::RegisterMetricsProvider(
    std::unique_ptr<metrics::MetricsProvider> provider) {
  metrics_providers_.RegisterMetricsProvider(std::move(provider));
}

// static
void UkmService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterUint64Pref(prefs::kUkmClientId, 0);
  registry->RegisterIntegerPref(prefs::kUkmSessionId, 0);
  UkmReportingService::RegisterPrefs(registry);
}

void UkmService::StartInitTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::StartInitTask";
  metrics_providers_.AsyncInit(base::Bind(&UkmService::FinishedInitTask,
                                          self_ptr_factory_.GetWeakPtr()));
}

void UkmService::FinishedInitTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::FinishedInitTask";
  initialize_complete_ = true;
  scheduler_->InitTaskComplete();
  if (initialization_complete_callback_) {
    std::move(initialization_complete_callback_).Run();
  }
}

void UkmService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::RotateLog";
  if (!reporting_service_.ukm_log_store()->has_unsent_logs())
    BuildAndStoreLog();
  reporting_service_.Start();
  scheduler_->RotationFinished();
}

void UkmService::AddSyncedUserNoiseBirthYearAndGenderToReport(Report* report) {
  if (!base::FeatureList::IsEnabled(kReportUserNoisedUserBirthYearAndGender))
    return;

  demographics_provider_->ProvideSyncedUserNoisedBirthYearAndGenderToReport(
      report);
}

void UkmService::BuildAndStoreLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::BuildAndStoreLog";

  // Suppress generating a log if we have no new data to include.
  bool empty = sources().empty() && entries().empty();
  UMA_HISTOGRAM_BOOLEAN("UKM.BuildAndStoreLogIsEmpty", empty);
  if (empty)
    return;

  Report report;
  report.set_client_id(client_id_);
  report.set_session_id(session_id_);
  report.set_report_id(++report_count_);

  StoreRecordingsInReport(&report);

  metrics::MetricsLog::RecordCoreSystemProfile(client_,
                                               report.mutable_system_profile());

  metrics_providers_.ProvideSystemProfileMetricsWithLogCreationTime(
      log_creation_time_, report.mutable_system_profile());

  AddSyncedUserNoiseBirthYearAndGenderToReport(&report);

  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  reporting_service_.ukm_log_store()->StoreLog(serialized_log);
}

bool UkmService::ShouldRestrictToWhitelistedEntries() const {
  return restrict_to_whitelist_entries_;
}

void UkmService::SetInitializationCompleteCallbackForTesting(base::OnceClosure callback) {
  if (initialize_complete_) {
    std::move(callback).Run();
  } else {
    // Store the callback to be invoked when initialization is complete later.
    initialization_complete_callback_ = std::move(callback);
  }
}

const base::Feature UkmService::kReportUserNoisedUserBirthYearAndGender = {
    "UkmReportNoisedUserBirthYearAndGender", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace ukm
