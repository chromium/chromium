// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/hats_office_trigger.h"

#include "base/check_op.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/hats/hats_notification_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace ash::cloud_upload {

namespace {
constexpr char kHatsOfficeGroup[] = "group";

const char* HatsOfficeGroupToString(HatsOfficeGroup group) {
  switch (group) {
    case HatsOfficeGroup::kDrive:
      return "drive";
    case HatsOfficeGroup::kMS365:
      return "ms365";
    case HatsOfficeGroup::kQuickOffice:
      return "quickoffice";
    case HatsOfficeGroup::kQuickOfficeClippyOff:
      return "quickoffice-clippyoff";
  }
}

}  // namespace

// static
HatsOfficeTrigger& HatsOfficeTrigger::Get() {
  static base::NoDestructor<HatsOfficeTrigger> instance;
  return *instance;
}

HatsOfficeTrigger::HatsOfficeTrigger() = default;
HatsOfficeTrigger::~HatsOfficeTrigger() = default;

void HatsOfficeTrigger::ShowSurveyAfterDelay(HatsOfficeGroup group) {
  if (show_survey_callback_for_testing_) {
    // In tests, just check that this method has been called with the right
    // "group" survey metadata.
    std::move(show_survey_callback_for_testing_).Run(group);
    return;
  }
  // The user has already seen a survey or we're about to show them one.
  if (hats_notification_controller_ || hats_notification_timer_.IsRunning()) {
    return;
  }

  hats_notification_timer_.Start(
      FROM_HERE, kHatsSurveyTimeout,
      base::BindOnce(&HatsOfficeTrigger::ShowSurveyIfSelected,
                     weak_ptr_factory_.GetWeakPtr(), group));
}

void HatsOfficeTrigger::SetShowSurveyAfterDelayCallbackForTesting(
    ShowSurveyCallbackForTesting callback) {
  show_survey_callback_for_testing_ = std::move(callback);
}

const HatsNotificationController*
HatsOfficeTrigger::GetHatsNotificationControllerForTesting() const {
  return hats_notification_controller_.get();
}

base::OneShotTimer& HatsOfficeTrigger::GetTimerForTesting() {
  return hats_notification_timer_;
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

void HatsOfficeTrigger::ShowSurveyIfSelected(HatsOfficeGroup group) {
  // We only show the survey if the current session is still active.
  if (session_manager::SessionManager::Get()->IsUserSessionBlocked()) {
    return;
  }
  Profile* profile = GetProfile();
  if (!profile) {
    // This can happen in tests when there is no `ProfileManager` instance.
    return;
  }
  // Disable survey for managed accounts.
  if (profile->GetProfilePolicyConnector()->IsManaged()) {
    return;
  }
  if (!HatsNotificationController::ShouldShowSurveyToProfile(
          profile, kHatsOfficeSurvey)) {
    return;
  }
  const base::flat_map<std::string, std::string> product_specific_data = {
      {kHatsOfficeGroup, HatsOfficeGroupToString(group)}};
  hats_notification_controller_ =
      base::MakeRefCounted<ash::HatsNotificationController>(
          profile, kHatsOfficeSurvey, product_specific_data);
}

}  // namespace ash::cloud_upload
