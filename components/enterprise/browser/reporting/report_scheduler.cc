// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_scheduler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

const int kMaximumRetry = 10;  // Retry 10 times takes about 15 to 19 hours.

bool IsBrowserVersionUploaded(ReportScheduler::ReportTrigger trigger) {
  switch (trigger) {
    case ReportScheduler::kTriggerTimer:
    case ReportScheduler::kTriggerManual:
    case ReportScheduler::kTriggerUpdate:
    case ReportScheduler::kTriggerNewVersion:
      return true;
    case ReportScheduler::kTriggerNone:
      return false;
  }
}

}  // namespace

ReportScheduler::Delegate::Delegate() = default;
ReportScheduler::Delegate::~Delegate() = default;

ReportScheduler::CreateParams::CreateParams() = default;
ReportScheduler::CreateParams::CreateParams(
    ReportScheduler::CreateParams&& other) = default;
ReportScheduler::CreateParams& ReportScheduler::CreateParams::operator=(
    ReportScheduler::CreateParams&& other) = default;
ReportScheduler::CreateParams::~CreateParams() = default;

void ReportScheduler::Delegate::SetReportTriggerCallback(
    ReportScheduler::ReportTriggerCallback callback) {
  DCHECK(trigger_report_callback_.is_null());
  trigger_report_callback_ = std::move(callback);
}

ReportScheduler::ReportScheduler(CreateParams params)
    : delegate_(std::move(params.delegate)),
      cloud_policy_client_(params.client),
      report_generator_(std::move(params.report_generator)),
      profile_request_generator_(std::move(params.profile_request_generator)),
      real_time_report_controller_(
          std::move(params.real_time_report_controller)) {
  DCHECK(cloud_policy_client_);
  DCHECK(delegate_);

  if (report_generator_) {
    reporting_pref_name_ = std::string(kCloudReportingEnabled);
    full_report_type_ = ReportType::kFull;
  } else {
    reporting_pref_name_ = std::string(kCloudProfileReportingEnabled);
    full_report_type_ = ReportType::kProfileReport;
  }

  delegate_->SetReportTriggerCallback(
      base::BindRepeating(&ReportScheduler::GenerateAndUploadReport,
                          weak_ptr_factory_.GetWeakPtr()));
  RegisterPrefObserver();
}

ReportScheduler::~ReportScheduler() = default;

bool ReportScheduler::IsReportingEnabled() const {
  return delegate_->GetPrefService()->GetBoolean(reporting_pref_name_);
}

bool ReportScheduler::IsNextReportScheduledForTesting() const {
  return request_timer_.IsRunning();
}

ReportScheduler::ReportTrigger ReportScheduler::GetActiveTriggerForTesting()
    const {
  return active_trigger_;
}

void ReportScheduler::SetReportUploaderForTesting(
    std::unique_ptr<ReportUploader> uploader) {
  report_uploader_ = std::move(uploader);
}

ReportScheduler::Delegate* ReportScheduler::GetDelegateForTesting() {
  return delegate_.get();
}

void ReportScheduler::OnDMTokenUpdated() {
  OnReportEnabledPrefChanged();
  if (real_time_report_controller_) {
    real_time_report_controller_->OnDMTokenUpdated(GetDMToken());
  }
}

void ReportScheduler::UploadFullReport(base::OnceClosure on_report_uploaded) {
  if (!IsReportingEnabled()) {
    VLOG(1) << "Reporting is not enabled.";
    std::move(on_report_uploaded).Run();
    return;
  }

  if (on_manual_report_uploaded_) {
    VLOG(1) << "Another report is uploading.";
    std::move(on_report_uploaded).Run();
    return;
  }
  on_manual_report_uploaded_ = std::move(on_report_uploaded);
  GenerateAndUploadReport(kTriggerManual);
}

void ReportScheduler::RegisterPrefObserver() {
  pref_change_registrar_.Init(delegate_->GetPrefService());
  pref_change_registrar_.Add(
      reporting_pref_name_,
      base::BindRepeating(&ReportScheduler::OnReportEnabledPrefChanged,
                          base::Unretained(this)));
  // Trigger first pref check during launch process.
  OnDMTokenUpdated();
}

void ReportScheduler::OnReportEnabledPrefChanged() {
  if (!IsReportingEnabled()) {
    Stop();
    return;
  }

  // For Chrome OS, it needn't register the cloud policy client here. The
  // |dm_token| and |client_id| should have already existed after the client is
  // initialized, and will keep valid during whole life-cycle.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!SetupBrowserPolicyClientRegistration()) {
    Stop();
    return;
  }
#endif

  // Start the periodic report timer.
  RestartReportTimer();
  if (!pref_change_registrar_.IsObserved(kCloudReportingUploadFrequency)) {
    pref_change_registrar_.Add(
        kCloudReportingUploadFrequency,
        base::BindRepeating(&ReportScheduler::RestartReportTimer,
                            base::Unretained(this)));
  }

  // Only device report generator support real time partial version report with
  // DM Server. With longer term, this should use `real_time_report_controller_`
  // instead.
  if (report_generator_) {
    delegate_->StartWatchingUpdatesIfNeeded(
        delegate_->GetPrefService()->GetTime(kLastUploadTimestamp),
        delegate_->GetPrefService()->GetTimeDelta(
            kCloudReportingUploadFrequency));
  }
}

void ReportScheduler::Stop() {
  request_timer_.Stop();
  if (report_generator_)
    delegate_->StopWatchingUpdates();
  report_uploader_.reset();
  if (pref_change_registrar_.IsObserved(kCloudReportingUploadFrequency))
    pref_change_registrar_.Remove(kCloudReportingUploadFrequency);
}

void ReportScheduler::RestartReportTimer() {
  request_timer_.Stop();
  Start(delegate_->GetPrefService()->GetTime(kLastUploadTimestamp));
}

bool ReportScheduler::SetupBrowserPolicyClientRegistration() {
  if (cloud_policy_client_->is_registered())
    return true;

  auto dm_token = GetDMToken();
  std::string client_id;
  if (profile_request_generator_) {
    // Get token for profile reporting
    client_id = delegate_->GetProfileClientId();
  } else {
    // Get token for browser reporting
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#else
    NOTREACHED_IN_MIGRATION();
    return true;
#endif
  }
  if (!dm_token.is_valid() || client_id.empty()) {
    VLOG(1)
        << "Enterprise reporting is disabled because device or profile is not "
           "enrolled.";
    return false;
  }

  cloud_policy_client_->SetupRegistration(
      dm_token.value(), client_id,
      /*user_affiliation_ids=*/std::vector<std::string>());
  return true;
}

void ReportScheduler::Start(base::Time last_upload_time) {
  // The next report is triggered 24h after the previous was uploaded.
  const base::Time next_upload_time =
      last_upload_time +
      delegate_->GetPrefService()->GetTimeDelta(kCloudReportingUploadFrequency);
  if (VLOG_IS_ON(1)) {
    base::TimeDelta first_request_delay = next_upload_time - base::Time::Now();
    VLOG(1) << "Schedule the first report in about "
            << first_request_delay.InHours() << " hour(s) and "
            << first_request_delay.InMinutes() % 60 << " minute(s).";
  }
  request_timer_.Start(FROM_HERE, next_upload_time,
                       base::BindOnce(&ReportScheduler::GenerateAndUploadReport,
                                      base::Unretained(this), kTriggerTimer));
}

void ReportScheduler::GenerateAndUploadReport(ReportTrigger trigger) {
  if (active_trigger_ != kTriggerNone) {
    // A report is already being generated. Remember this trigger to be handled
    // once the current report completes.
    pending_triggers_ |= trigger;
    return;
  }

  active_trigger_ = trigger;
  ReportType report_type = TriggerToReportType(trigger);
  if (report_type == ReportType::kProfileReport) {
    DCHECK(profile_request_generator_);
    profile_request_generator_->Generate(base::BindOnce(
        &ReportScheduler::OnReportGenerated, base::Unretained(this)));
  } else {
    DCHECK(report_generator_);
    report_generator_->Generate(
        report_type, base::BindOnce(&ReportScheduler::OnReportGenerated,
                                    base::Unretained(this)));
  }
}

void ReportScheduler::OnReportGenerated(ReportRequestQueue requests) {
  DCHECK_NE(active_trigger_, kTriggerNone);
  if (requests.empty()) {
    SYSLOG(ERROR)
        << "No cloud report can be generated. Likely the report is too large.";
    // Do not restart the periodic report timer, as it's likely that subsequent
    // attempts to generate full reports would also fail.
    active_trigger_ = kTriggerNone;
    RunPendingTriggers();
    return;
  }
  VLOG(1) << "Uploading enterprise report.";
  if (!report_uploader_) {
    report_uploader_ =
        std::make_unique<ReportUploader>(cloud_policy_client_, kMaximumRetry);
  }
  RecordUploadTrigger(active_trigger_);
  report_uploader_->SetRequestAndUpload(
      TriggerToReportType(active_trigger_), std::move(requests),
      base::BindOnce(&ReportScheduler::OnReportUploaded,
                     base::Unretained(this)));
}

void ReportScheduler::OnReportUploaded(ReportUploader::ReportStatus status) {
  DCHECK_NE(active_trigger_, kTriggerNone);
  VLOG(1) << "The enterprise report upload result " << status << ".";
  switch (status) {
    case ReportUploader::kSuccess:
      // Schedule the next report for success. Reset uploader to reset failure
      // count.
      report_uploader_.reset();
      if (IsBrowserVersionUploaded(active_trigger_))
        delegate_->OnBrowserVersionUploaded();

      delegate_->GetPrefService()->SetTime(kLastUploadSucceededTimestamp,
                                           base::Time::Now());
      [[fallthrough]];
    case ReportUploader::kTransientError:
      // Stop retrying and schedule the next report to avoid stale report.
      // Failure count is not reset so retry delay remains.
      if (active_trigger_ == kTriggerTimer ||
          active_trigger_ == kTriggerManual) {
        const base::Time now = base::Time::Now();
        delegate_->GetPrefService()->SetTime(kLastUploadTimestamp, now);
        if (IsReportingEnabled())
          Start(now);
      }
      break;
    case ReportUploader::kPersistentError:
      Stop();
      // No future upload until Chrome relaunch or pref change event.
      break;
  }

  if ((active_trigger_ == kTriggerManual || active_trigger_ == kTriggerTimer)) {
    if (on_manual_report_uploaded_)
      std::move(on_manual_report_uploaded_).Run();

    // Timer and Manual report are exactly same. If we just uploaded one, skip
    // the other.
    if (pending_triggers_ & kTriggerTimer)
      pending_triggers_ -= kTriggerTimer;
    if (pending_triggers_ & kTriggerManual)
      pending_triggers_ -= kTriggerManual;
  }
  active_trigger_ = kTriggerNone;
  RunPendingTriggers();
}

void ReportScheduler::RunPendingTriggers() {
  DCHECK_EQ(active_trigger_, kTriggerNone);
  if (!pending_triggers_)
    return;

  // Timer-triggered reports are a superset of those triggered by an update or a
  // new version, so favor them and consider that they serve all purposes.

  ReportTrigger trigger;
  if ((pending_triggers_ & kTriggerTimer) != 0) {
    // Timer-triggered reports contain data of all other report types.
    trigger = kTriggerTimer;
    pending_triggers_ = 0;
  } else if ((pending_triggers_ & kTriggerManual) != 0) {
    // Manual-triggered reports also contains all data.
    trigger = kTriggerManual;
    pending_triggers_ = 0;
  } else {
    trigger = (pending_triggers_ & kTriggerUpdate) != 0 ? kTriggerUpdate
                                                        : kTriggerNewVersion;
    pending_triggers_ = 0;
  }

  GenerateAndUploadReport(trigger);
}

// static
void ReportScheduler::RecordUploadTrigger(ReportTrigger trigger) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Sample {
    kNone = 0,
    kTimer = 1,
    kUpdate = 2,
    kNewVersion = 3,
    kExtensionRequest = 4,          // Deprecated.
    kExtensionRequestRealTime = 5,  // Deprecated.
    kManual = 6,
    kMaxValue = kManual
  } sample = Sample::kNone;
  switch (trigger) {
    case kTriggerNone:
      break;
    case kTriggerTimer:
      sample = Sample::kTimer;
      break;
    case kTriggerManual:
      sample = Sample::kManual;
      break;
    case kTriggerUpdate:
      sample = Sample::kUpdate;
      break;
    case kTriggerNewVersion:
      sample = Sample::kNewVersion;
      break;
  }
  base::UmaHistogramEnumeration("Enterprise.CloudReportingUploadTrigger",
                                sample);
}

ReportType ReportScheduler::TriggerToReportType(
    ReportScheduler::ReportTrigger trigger) {
  switch (trigger) {
    case ReportScheduler::kTriggerNone:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case ReportScheduler::kTriggerTimer:
    case ReportScheduler::kTriggerManual:
      return full_report_type_;
    case ReportScheduler::kTriggerUpdate:
      return ReportType::kBrowserVersion;
    case ReportScheduler::kTriggerNewVersion:
      return ReportType::kBrowserVersion;
  }
}

policy::DMToken ReportScheduler::GetDMToken() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return policy::DMToken::CreateValidToken(cloud_policy_client_->dm_token());
#else
  if (profile_request_generator_) {
    return delegate_->GetProfileDMToken();
  } else {
    return policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  }
#endif
}

}  // namespace enterprise_reporting
