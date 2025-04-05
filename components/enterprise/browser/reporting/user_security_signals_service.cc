// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/user_security_signals_service.h"

#include "base/check.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr base::TimeDelta kSecurityUploadsInterval = base::Hours(4);

}

namespace enterprise_reporting {

UserSecuritySignalsService::UserSecuritySignalsService(
    PrefService* profile_prefs,
    Delegate* delegate)
    : profile_prefs_(profile_prefs), delegate_(delegate) {
  CHECK(profile_prefs_);
  CHECK(delegate_);
}

UserSecuritySignalsService::~UserSecuritySignalsService() = default;

base::TimeDelta UserSecuritySignalsService::GetSecurityUploadCadence() {
  return kSecurityUploadsInterval;
}

void UserSecuritySignalsService::Start() {
  if (initialized_) {
    return;
  }

  initialized_ = true;

  pref_change_registrar_.Init(profile_prefs_);
  pref_change_registrar_.Add(
      kUserSecuritySignalsReporting,
      base::BindRepeating(
          &UserSecuritySignalsService::OnStatePolicyValueChanged,
          weak_factory_.GetWeakPtr()));

  // Manually trigger a policy update to initialize things.
  OnStatePolicyValueChanged();
}

bool UserSecuritySignalsService::IsSecuritySignalsReportingEnabled() {
  return profile_prefs_->GetBoolean(kUserSecuritySignalsReporting);
}

bool UserSecuritySignalsService::ShouldUseCookies() {
  return IsSecuritySignalsReportingEnabled() &&
         profile_prefs_->GetBoolean(kUserSecurityAuthenticatedReporting);
}

void UserSecuritySignalsService::OnReportUploaded() {
  timer_.Start(FROM_HERE, base::Time::Now() + GetSecurityUploadCadence(),
               base::BindRepeating(&UserSecuritySignalsService::TriggerReport,
                                   weak_factory_.GetWeakPtr(),
                                   SecurityReportTrigger::kTimer));
}

void UserSecuritySignalsService::OnStatePolicyValueChanged() {
  if (!IsSecuritySignalsReportingEnabled()) {
    timer_.Stop();
    return;
  }

  if (timer_.IsRunning()) {
    return;
  }

  // The policy is enabled and the timed loop isn't running. Send an upload
  // immediately.
  TriggerReport(SecurityReportTrigger::kTimer);
}

void UserSecuritySignalsService::TriggerReport(SecurityReportTrigger trigger) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &Delegate::OnReportEventTriggered, base::Unretained(delegate_),
          trigger,
          base::BindOnce(&UserSecuritySignalsService::OnReportUploaded,
                         weak_factory_.GetWeakPtr())));
}

}  // namespace enterprise_reporting
