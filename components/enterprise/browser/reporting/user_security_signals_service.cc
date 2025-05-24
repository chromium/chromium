// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/user_security_signals_service.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_change_dispatcher.h"

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
  return enterprise_signals::features::kProfileSignalsReportingInterval.Get();
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
  pref_change_registrar_.Add(
      kUserSecurityAuthenticatedReporting,
      base::BindRepeating(
          &UserSecuritySignalsService::OnCookiePolicyValueChanged,
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

void UserSecuritySignalsService::OnCookieChange(
    const net::CookieChangeInfo& change) {
  if (!enterprise_signals::features::kTriggerOnCookieChange.Get()) {
    return;
  }
  // Only trigger a new upload if the cookie change could result in a new or
  // updated session.
  if (change.cause == net::CookieChangeCause::INSERTED) {
    TriggerReport(SecurityReportTrigger::kCookieChange);
  }
}

void UserSecuritySignalsService::InitCookieListener() {
  auto* cookie_manager = delegate_->GetCookieManager();
  if (!cookie_manager) {
    return;
  }

  // Only interested in the secure first-party authentication cookie.
  cookie_manager->AddCookieChangeListener(
      GaiaUrls::GetInstance()->secure_google_url(),
      GaiaConstants::kGaiaSigninCookieName,
      cookie_listener_receiver_.BindNewPipeAndPassRemote());

  cookie_listener_receiver_.set_disconnect_handler(base::BindOnce(
      &UserSecuritySignalsService::OnCookieListenerConnectionError,
      base::Unretained(this)));
}

void UserSecuritySignalsService::OnCookieListenerConnectionError() {
  // A connection error from the CookieManager likely means that the network
  // service process has crashed. Try again to set up a listener.
  cookie_listener_receiver_.reset();
  InitCookieListener();
}

void UserSecuritySignalsService::OnStatePolicyValueChanged() {
  if (!IsSecuritySignalsReportingEnabled()) {
    timer_.Stop();
    return;
  }

  if (timer_.IsRunning()) {
    return;
  }

  // Make sure that cookie observation is properly set-up, as needed.
  OnCookiePolicyValueChanged();

  // The policy is enabled and the timed loop isn't running. Send an upload
  // immediately.
  TriggerReport(SecurityReportTrigger::kTimer);
}

void UserSecuritySignalsService::OnCookiePolicyValueChanged() {
  if (ShouldUseCookies() && !cookie_listener_receiver_.is_bound()) {
    InitCookieListener();
  } else if (!ShouldUseCookies() && cookie_listener_receiver_.is_bound()) {
    cookie_listener_receiver_.reset();
  }
}

void UserSecuritySignalsService::TriggerReport(SecurityReportTrigger trigger) {
  VLOG_POLICY(1, REPORTING) << "Security signals report is triggered by "
                            << static_cast<int>(trigger);
  base::UmaHistogramEnumeration("Enterprise.SecurityReport.User.Trigger",
                                trigger);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::OnReportEventTriggered,
                                base::Unretained(delegate_), trigger));
}

}  // namespace enterprise_reporting
