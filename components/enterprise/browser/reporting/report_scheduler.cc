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
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_generation_config.h"
#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

const int kMaximumRetry = 10;  // Retry 10 times takes about 15 to 19 hours.

bool IsBrowserVersionUploaded(ReportTrigger trigger) {
  switch (trigger) {
    case ReportTrigger::kTriggerTimer:
    case ReportTrigger::kTriggerManual:
    case ReportTrigger::kTriggerUpdate:
    case ReportTrigger::kTriggerNewVersion:
    case ReportTrigger::kTriggerSecurity:
      return true;
    case ReportTrigger::kTriggerNone:
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

  delegate_->OnInitializationCompleted();
}

ReportScheduler::~ReportScheduler() = default;

bool ReportScheduler::IsReportingEnabled() const {
  return delegate_->GetPrefService()->GetBoolean(reporting_pref_name_);
}

bool ReportScheduler::AreSecurityReportsEnabled() const {
  return delegate_->AreSecurityReportsEnabled();
}

bool ReportScheduler::IsNextReportScheduledForTesting() const {
  return request_timer_.IsRunning();
}

ReportTrigger ReportScheduler::GetActiveTriggerForTesting() const {
  return active_report_generation_config_.report_trigger;
}

ReportGenerationConfig ReportScheduler::GetActiveGenerationConfigForTesting()
    const {
  return active_report_generation_config_;
}

void ReportScheduler::QueueReportUploaderForTesting(
    std::unique_ptr<ReportUploader> uploader) {
  report_uploaders_for_test_.push_back(std::move(uploader));
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
  ReportTrigger trigger = kTriggerNone;
  if (IsReportingEnabled()) {
    trigger = kTriggerManual;
  } else if (delegate_->AreSecurityReportsEnabled()) {
    trigger = kTriggerSecurity;
  } else {
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
  GenerateAndUploadReport(trigger);
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
#if !BUILDFLAG(IS_CHROMEOS)
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
#if !BUILDFLAG(IS_CHROMEOS)
    client_id = policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#else
    NOTREACHED();
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
  if (delegate_->AreSecurityReportsEnabled()) {
    // Does nothing if client is already registered.
    SetupBrowserPolicyClientRegistration();
  }

  if (active_report_generation_config_.report_trigger != kTriggerNone) {
    // A report is already being generated. Remember this trigger to be handled
    // once the current report completes.
    pending_triggers_ |= trigger;
    return;
  }

  ReportType report_type = TriggerToReportType(trigger);
  SecuritySignalsMode signals_mode = SecuritySignalsMode::kNoSignals;
  if (report_type == ReportType::kProfileReport) {
    signals_mode = delegate_->AreSecurityReportsEnabled()
                       ? (trigger == ReportTrigger::kTriggerSecurity
                              ? SecuritySignalsMode::kSignalsOnly
                              : SecuritySignalsMode::kSignalsAttached)
                       : SecuritySignalsMode::kNoSignals;
  }

  active_report_generation_config_ = ReportGenerationConfig(
      trigger, report_type, signals_mode, delegate_->UseCookiesInUploads());

  VLOG_POLICY(1, REPORTING)
      << "Starting report generation with the following configuration: "
      << active_report_generation_config_.ToString();

  if (report_type == ReportType::kProfileReport) {
    DCHECK(profile_request_generator_);
    profile_request_generator_->Generate(
        active_report_generation_config_,
        base::BindOnce(&ReportScheduler::OnReportGenerated,
                       base::Unretained(this)));
  } else {
    DCHECK(report_generator_);
    report_generator_->Generate(
        active_report_generation_config_.report_type,
        base::BindOnce(&ReportScheduler::OnReportGenerated,
                       base::Unretained(this)));
  }
}

void ReportScheduler::OnReportGenerated(ReportRequestQueue requests) {
  DCHECK_NE(active_report_generation_config_.report_trigger,
            ReportTrigger::kTriggerNone);
  if (requests.empty()) {
    SYSLOG(ERROR)
        << "No cloud report can be generated. Likely the report is too large.";
    // Do not restart the periodic report timer, as it's likely that subsequent
    // attempts to generate full reports would also fail.
    active_report_generation_config_ =
        ReportGenerationConfig(ReportTrigger::kTriggerNone);
    RunPendingTriggers();
    return;
  }
  VLOG(1) << "Uploading enterprise report.";
  if (!report_uploader_ && report_uploaders_for_test_.size() > 0) {
    report_uploader_ = std::move(report_uploaders_for_test_.front());
    report_uploaders_for_test_.erase(report_uploaders_for_test_.begin());
  } else if (!report_uploader_) {
    report_uploader_ =
        std::make_unique<ReportUploader>(cloud_policy_client_, kMaximumRetry);
  }

  RecordUploadTrigger();
  if (active_report_generation_config_.security_signals_mode !=
      SecuritySignalsMode::kNoSignals) {
    delegate_->GetPrefService()->SetTime(kLastSignalsUploadAttemptTimestamp,
                                         base::Time::Now());
  }

  report_uploader_->SetRequestAndUpload(
      active_report_generation_config_, std::move(requests),
      base::BindOnce(&ReportScheduler::OnReportUploaded,
                     base::Unretained(this)));
}

void ReportScheduler::OnReportUploaded(ReportUploader::ReportStatus status) {
  DCHECK_NE(active_report_generation_config_.report_trigger,
            ReportTrigger::kTriggerNone);
  VLOG(1) << "The enterprise report upload result " << status << ".";
  switch (status) {
    case ReportUploader::kSuccess:
      // Schedule the next report for success. Reset uploader to reset failure
      // count.
      report_uploader_.reset();
      if (IsBrowserVersionUploaded(
              active_report_generation_config_.report_trigger)) {
        delegate_->OnBrowserVersionUploaded();
      }

      // Signals-only report does not contain most content of a status report
      // and should not update this timestamp.
      if (active_report_generation_config_.report_trigger !=
          ReportTrigger::kTriggerSecurity) {
        delegate_->GetPrefService()->SetTime(kLastUploadSucceededTimestamp,
                                             base::Time::Now());
      }

      if (active_report_generation_config_.security_signals_mode !=
          SecuritySignalsMode::kNoSignals) {
        delegate_->GetPrefService()->SetTime(
            kLastSignalsUploadSucceededTimestamp, base::Time::Now());
        delegate_->GetPrefService()->SetString(
            kLastSignalsUploadSucceededConfig,
            active_report_generation_config_.ToString());
      }
      [[fallthrough]];
    case ReportUploader::kTransientError:
      // Stop retrying and schedule the next report to avoid stale report.
      // Failure count is not reset so retry delay remains.
      if (active_report_generation_config_.report_trigger ==
              ReportTrigger::kTriggerTimer ||
          active_report_generation_config_.report_trigger ==
              ReportTrigger::kTriggerManual) {
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

  if ((active_report_generation_config_.report_trigger ==
           ReportTrigger::kTriggerManual ||
       active_report_generation_config_.report_trigger ==
           ReportTrigger::kTriggerTimer)) {
    // Timer and Manual report are exactly same. If we just uploaded one, skip
    // the other.
    if (pending_triggers_ & ReportTrigger::kTriggerTimer) {
      pending_triggers_ -= ReportTrigger::kTriggerTimer;
    }
    if (pending_triggers_ & ReportTrigger::kTriggerManual) {
      pending_triggers_ -= ReportTrigger::kTriggerManual;
    }
  }

  if (active_report_generation_config_.report_trigger ==
          ReportTrigger::kTriggerManual ||
      active_report_generation_config_.report_trigger ==
          ReportTrigger::kTriggerTimer ||
      active_report_generation_config_.report_trigger ==
          ReportTrigger::kTriggerSecurity) {
    if (on_manual_report_uploaded_) {
      std::move(on_manual_report_uploaded_).Run();
    }

    if (active_report_generation_config_.security_signals_mode !=
        SecuritySignalsMode::kNoSignals) {
      delegate_->OnSecuritySignalsUploaded();

      // A full report includes security signals already, we don't need another
      // security signals only report until the timer runs out again.
      if (pending_triggers_ & ReportTrigger::kTriggerSecurity) {
        pending_triggers_ -= ReportTrigger::kTriggerSecurity;
      }
    }
  }

  active_report_generation_config_ =
      ReportGenerationConfig(ReportTrigger::kTriggerNone);
  RunPendingTriggers();
}

void ReportScheduler::RunPendingTriggers() {
  DCHECK_EQ(active_report_generation_config_.report_trigger,
            ReportTrigger::kTriggerNone);
  if (!pending_triggers_)
    return;

  // Timer-triggered reports are a superset of those triggered by an update or a
  // new version, so favor them and consider that they serve all purposes.

  ReportTrigger trigger;
  if ((pending_triggers_ & ReportTrigger::kTriggerTimer) != 0) {
    // Timer-triggered reports contain data of all other report types.
    trigger = ReportTrigger::kTriggerTimer;
    pending_triggers_ = 0;
  } else if ((pending_triggers_ & ReportTrigger::kTriggerManual) != 0) {
    // Manual-triggered reports also contains all data.
    trigger = kTriggerManual;
    pending_triggers_ = 0;
  } else if ((pending_triggers_ & ReportTrigger::kTriggerSecurity) != 0) {
    trigger = kTriggerSecurity;
    pending_triggers_ -= ReportTrigger::kTriggerSecurity;
  } else {
    // Update and NewVersion triggers lead to the same report content being
    // uploaded.
    if ((pending_triggers_ & ReportTrigger::kTriggerUpdate) != 0) {
      trigger = ReportTrigger::kTriggerUpdate;
      pending_triggers_ -= ReportTrigger::kTriggerUpdate;
    }

    if ((pending_triggers_ & ReportTrigger::kTriggerNewVersion) != 0) {
      trigger = ReportTrigger::kTriggerNewVersion;
      pending_triggers_ -= ReportTrigger::kTriggerNewVersion;
    }
  }

  GenerateAndUploadReport(trigger);
}

void ReportScheduler::RecordUploadTrigger() {
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
    kSecurity = 7,
    kMaxValue = kSecurity
  } sample = Sample::kNone;
  switch (active_report_generation_config_.report_trigger) {
    case ReportTrigger::kTriggerNone:
      break;
    case ReportTrigger::kTriggerTimer:
      sample = Sample::kTimer;
      break;
    case ReportTrigger::kTriggerManual:
      sample = Sample::kManual;
      break;
    case ReportTrigger::kTriggerUpdate:
      sample = Sample::kUpdate;
      break;
    case ReportTrigger::kTriggerNewVersion:
      sample = Sample::kNewVersion;
      break;
    case ReportTrigger::kTriggerSecurity:
      sample = Sample::kSecurity;
      break;
  }
  base::UmaHistogramEnumeration("Enterprise.CloudReportingUploadTrigger",
                                sample);

  if (active_report_generation_config_.security_signals_mode !=
      SecuritySignalsMode::kNoSignals) {
    base::UmaHistogramEnumeration(
        "Enterprise.SecurityReport.User.Mode",
        active_report_generation_config_.security_signals_mode);
  }
}

ReportType ReportScheduler::TriggerToReportType(ReportTrigger trigger) {
  switch (trigger) {
    case ReportTrigger::kTriggerNone:
      NOTREACHED();
    case ReportTrigger::kTriggerTimer:
    case ReportTrigger::kTriggerManual:
      return full_report_type_;
    case ReportTrigger::kTriggerUpdate:
      return ReportType::kBrowserVersion;
    case ReportTrigger::kTriggerNewVersion:
      return ReportType::kBrowserVersion;
    case ReportTrigger::kTriggerSecurity:
      // Security triggers are not supported at the browser-level yet.
      return ReportType::kProfileReport;
  }
}

policy::DMToken ReportScheduler::GetDMToken() {
#if BUILDFLAG(IS_CHROMEOS)
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
