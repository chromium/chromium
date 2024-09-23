// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash::cloud_upload {
namespace {

constexpr char kHatsOfficeLaunchingApp[] = "Launching app";

const char* HatsOfficeLaunchingAppToString(HatsOfficeLaunchingApp app) {
  switch (app) {
    case HatsOfficeLaunchingApp::kDrive:
      return "Google Drive";
    case HatsOfficeLaunchingApp::kMS365:
      return "Microsoft 365";
    case HatsOfficeLaunchingApp::kQuickOffice:
      return "Quickoffice";
    case HatsOfficeLaunchingApp::kQuickOfficeClippyOff:
      return "Quickoffice (ChromeOS Office integration disabled)";
  }
}

}  // namespace

// static
HatsOfficeTrigger& HatsOfficeTrigger::Get() {
  static base::NoDestructor<HatsOfficeTrigger> instance;
  return *instance;
}

void HatsOfficeTrigger::SetShowSurveyCallbackForTesting(
    ShowSurveyCallbackForTesting callback) {
  show_survey_callback_for_testing_ = std::move(callback);
}

void HatsOfficeTrigger::ShowSurveyAfterDelay(HatsOfficeLaunchingApp app) {
  if (show_survey_callback_for_testing_) {
    // In tests, just check that this method has been called with the right
    // "Launching app" survey metadata.
    std::move(show_survey_callback_for_testing_).Run(std::string(), app);
    return;
  }
  if (!ShouldShowSurvey()) {
    return;
  }
  delay_trigger_ = std::make_unique<DelayTrigger>(
      base::BindOnce(&HatsOfficeTrigger::ShowSurveyIfSelected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(app)));
}

void HatsOfficeTrigger::ShowSurveyAfterAppInactive(const std::string& app_id,
                                                   HatsOfficeLaunchingApp app) {
  if (show_survey_callback_for_testing_) {
    // In tests, just check that this method has been called with the right
    // "Launching app" survey metadata.
    std::move(show_survey_callback_for_testing_).Run(app_id, app);
    return;
  }
  if (!ShouldShowSurvey()) {
    return;
  }
  Profile* profile = GetProfile();
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return;
  }
  app_state_trigger_ = std::make_unique<AppStateTrigger>(
      profile, app_id,
      base::BindOnce(&HatsOfficeTrigger::ShowSurveyIfSelected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(app)),
      base::BindOnce(&HatsOfficeTrigger::CleanupTriggers,
                     weak_ptr_factory_.GetWeakPtr()));
}

HatsOfficeTrigger::DelayTrigger::DelayTrigger(base::OnceClosure callback) {
  notification_timer_.Start(FROM_HERE, kDelayTriggerTimeout,
                            std::move(callback));
}

HatsOfficeTrigger::AppStateTrigger::AppStateTrigger(
    Profile* profile,
    const std::string& app_id,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback)
    : app_id_(app_id),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {
  apps::InstanceRegistry& registry =
      apps::AppServiceProxyFactory::GetForProfile(profile)->InstanceRegistry();
  observation_.Observe(&registry);
  // Start a timer to abandon the app state observation if the expected initial
  // event hasn't been received after a delay.
  first_app_state_event_timer_.Start(
      FROM_HERE, kFirstAppStateEventTimeout,
      base::BindOnce(&AppStateTrigger::StopTrackingAppState,
                     weak_ptr_factory_.GetWeakPtr()));
}
HatsOfficeTrigger::AppStateTrigger::~AppStateTrigger() = default;

void HatsOfficeTrigger::AppStateTrigger::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (update.AppId() != app_id_) {
    return;
  }
  // Assume that the first "started and running" update is from the right
  // `app_id_` instance.
  if (instance_id_.is_empty() &&
      update.State() == apps::InstanceState(apps::kStarted | apps::kRunning)) {
    instance_id_ = update.InstanceId();
  } else if (instance_id_ != update.InstanceId()) {
    return;
  }
  // Only check the app state if it hasn't changed within the given delay.
  debounce_timer_.Start(
      FROM_HERE, kDebounceDelay,
      base::BindOnce(&AppStateTrigger::HandleObservedAppStateUpdate,
                     weak_ptr_factory_.GetWeakPtr(), update.State()));
}

void HatsOfficeTrigger::AppStateTrigger::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  std::move(failure_callback_).Run();
}

void HatsOfficeTrigger::AppStateTrigger::HandleObservedAppStateUpdate(
    apps::InstanceState state) {
  if (!(state & apps::kActive) || (state & apps::kDestroyed)) {
    std::move(success_callback_).Run();
  }
}

void HatsOfficeTrigger::AppStateTrigger::StopTrackingAppState() {
  if (instance_id_.is_empty()) {
    // The trigger is still listening for `app_id_` events, but none have been
    // received since `instance_id_` isn't set yet. Abort by calling the failure
    // callback (`this` is getting destroyed).
    std::move(failure_callback_).Run();
  }
}

HatsOfficeTrigger::HatsOfficeTrigger() = default;
HatsOfficeTrigger::~HatsOfficeTrigger() = default;

bool HatsOfficeTrigger::ShouldShowSurvey() const {
  // The user has already seen a survey or we're about to show them one.
  if (hats_notification_controller_ || delay_trigger_ || app_state_trigger_) {
    return false;
  }
  Profile* profile = GetProfile();
  if (!profile) {
    // This can happen in tests when there is no `ProfileManager` instance.
    return false;
  }
  // Disable survey for managed accounts.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return false;
  }
  return true;
}

const HatsNotificationController*
HatsOfficeTrigger::GetHatsNotificationControllerForTesting() const {
  return hats_notification_controller_.get();
}
bool HatsOfficeTrigger::IsDelayTriggerActiveForTesting() {
  return delay_trigger_.get() != nullptr;
}
bool HatsOfficeTrigger::IsAppStateTriggerActiveForTesting() {
  return app_state_trigger_.get() != nullptr;
}

Profile* HatsOfficeTrigger::GetProfile() const {
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  // Don't record UMA if there is no primary user.
  if (!active_user) {
    return nullptr;
  }

  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user));
}

void HatsOfficeTrigger::ShowSurveyIfSelected(HatsOfficeLaunchingApp app) {
  CleanupTriggers();
  // We only show the survey if the current session is still active.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }
  Profile* profile = GetProfile();
  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsOfficeSurvey)) {
    return;
  }
  const base::flat_map<std::string, std::string> product_specific_data = {
      {kHatsOfficeLaunchingApp, HatsOfficeLaunchingAppToString(app)}};
  hats_notification_controller_ =
      base::MakeRefCounted<ash::HatsNotificationController>(
          profile, kHatsOfficeSurvey, product_specific_data);
}

void HatsOfficeTrigger::CleanupTriggers() {
  delay_trigger_.reset();
  app_state_trigger_.reset();
}

}  // namespace ash::cloud_upload
