// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <memory>
#include <string>
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
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/persisted_logs_metrics_impl.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_rotation_scheduler.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace ukm {

namespace {

// Generates a new client id and stores it in prefs.
uint64_t GenerateAndStoreClientId(PrefService* pref_service) {
  uint64_t client_id = 0;
  while (!client_id)
    client_id = base::RandUint64();
  pref_service->SetInt64(prefs::kUkmClientId, client_id);

  // Also reset the session id counter.
  pref_service->SetInteger(prefs::kUkmSessionId, 0);
  return client_id;
}

uint64_t LoadOrGenerateAndStoreClientId(PrefService* pref_service) {
  uint64_t client_id = pref_service->GetInt64(prefs::kUkmClientId);
  if (!client_id)
    client_id = GenerateAndStoreClientId(pref_service);
  return client_id;
}

int32_t LoadAndIncrementSessionId(PrefService* pref_service) {
  int32_t session_id = pref_service->GetInteger(prefs::kUkmSessionId);
  ++session_id;  // Increment session id, once per session.
  pref_service->SetInteger(prefs::kUkmSessionId, session_id);
  return session_id;
}

}  // namespace

UkmService::UkmService(PrefService* pref_service,
                       metrics::MetricsServiceClient* client,
                       bool restrict_to_whitelist_entries)
    : pref_service_(pref_service),
      restrict_to_whitelist_entries_(restrict_to_whitelist_entries),
      client_id_(0),
      session_id_(0),
      report_count_(0),
      client_(client),
      reporting_service_(client, pref_service),
      initialize_started_(false),
      initialize_complete_(false),
      self_ptr_factory_(this) {
  DCHECK(pref_service_);
  DCHECK(client_);
  DVLOG(1) << "UkmService::Constructor";

  reporting_service_.Initialize();

  base::Closure rotate_callback =
      base::Bind(&UkmService::RotateLog, self_ptr_factory_.GetWeakPtr());
  // MetricsServiceClient outlives UkmService, and
  // MetricsReportingScheduler is tied to the lifetime of |this|.
  const base::Callback<base::TimeDelta(void)>& get_upload_interval_callback =
      base::Bind(&metrics::MetricsServiceClient::GetStandardUploadInterval,
                 base::Unretained(client_));
  scheduler_.reset(new ukm::UkmRotationScheduler(rotate_callback,
                                                 get_upload_interval_callback));

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
  registry->RegisterInt64Pref(prefs::kUkmClientId, 0);
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
}

void UkmService::RotateLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "UkmService::RotateLog";
  if (!reporting_service_.ukm_log_store()->has_unsent_logs())
    BuildAndStoreLog();
  reporting_service_.Start();
  scheduler_->RotationFinished();
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

  metrics_providers_.ProvideSystemProfileMetrics(
      report.mutable_system_profile());

  std::string serialized_log;
  report.SerializeToString(&serialized_log);
  reporting_service_.ukm_log_store()->StoreLog(serialized_log);
}

bool UkmService::ShouldRestrictToWhitelistedEntries() const {
  return restrict_to_whitelist_entries_;
}

}  // namespace ukm
