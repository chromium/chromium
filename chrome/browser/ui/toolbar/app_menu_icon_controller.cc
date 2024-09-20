// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/check_op.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Maps an upgrade level to a severity level. When |show_very_low_upgrade_level|
// is true, VERY_LOW through HIGH all return Severity::LOW. Otherwise, VERY_LOW
// is ignored and LOW through HIGH return their respective Severity level, with
// GRACE treated the same as HIGH.
AppMenuIconController::Severity SeverityFromUpgradeLevel(
    bool show_very_low_upgrade_level,
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
  if (show_very_low_upgrade_level) {
    // Anything between kNone and kCritical is LOW for unstable desktop Chrome.
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
        break;
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
      case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
        return AppMenuIconController::Severity::LOW;
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::HIGH;
    }
  } else {
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
        break;
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
        // kVeryLow is meaningless for stable channels.
        return AppMenuIconController::Severity::NONE;
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
        return AppMenuIconController::Severity::LOW;
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
        return AppMenuIconController::Severity::MEDIUM;
      case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::HIGH;
    }
  }
  DCHECK_EQ(level, UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  return AppMenuIconController::Severity::NONE;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Return true if the browser is updating on the dev or canary channels.
bool IsUnstableChannel() {
  // Unbranded (Chromium) builds are on the UNKNOWN channel, so check explicitly
  // for the Google Chrome channels that are considered "unstable". This ensures
  // that Chromium builds get the default behavior.
  const version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::DEV ||
         channel == version_info::Channel::CANARY;
}

}  // namespace

AppMenuIconController::AppMenuIconController(Profile* profile,
                                             Delegate* delegate)
    : AppMenuIconController(UpgradeDetector::GetInstance(), profile, delegate) {
}

AppMenuIconController::AppMenuIconController(UpgradeDetector* upgrade_detector,
                                             Profile* profile,
                                             Delegate* delegate)
    : is_unstable_channel_(IsUnstableChannel()),
      upgrade_detector_(upgrade_detector),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);

  global_error_observation_.Observe(
      GlobalErrorServiceFactory::GetForProfile(profile_));
#if !BUILDFLAG(IS_CHROMEOS)
  default_browser_prompt_observation_.Observe(
      DefaultBrowserPromptManager::GetInstance());
#endif

  upgrade_detector_->AddObserver(this);
}

AppMenuIconController::~AppMenuIconController() {
  upgrade_detector_->RemoveObserver(this);
}

void AppMenuIconController::UpdateDelegate() {
  delegate_->UpdateTypeAndSeverity(GetTypeAndSeverity());
}

AppMenuIconController::TypeAndSeverity
AppMenuIconController::GetTypeAndSeverity() const {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser_defaults::kShowUpgradeMenuItem &&
      upgrade_detector_->notify_upgrade()) {
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level =
        upgrade_detector_->upgrade_notification_stage();
    // The severity may be NONE even if the detector has been notified of an
    // update. This can happen for beta and stable channels once the VERY_LOW
    // annoyance level is reached.
    auto severity = SeverityFromUpgradeLevel(is_unstable_channel_, level);
    if (severity != Severity::NONE)
      return {IconType::UPGRADE_NOTIFICATION, severity};
  }

  if (GlobalErrorServiceFactory::GetForProfile(profile_)
          ->GetHighestSeverityGlobalErrorWithAppMenuItem()) {
    // If you change the severity here, make sure to also change the menu icon
    // and the bubble icon.
    return {IconType::GLOBAL_ERROR, Severity::MEDIUM};
  }
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  if (DefaultBrowserPromptManager::GetInstance()->get_show_app_menu_prompt() &&
      !profile_->IsIncognitoProfile() && !profile_->IsGuestSession()) {
    CHECK(base::FeatureList::IsEnabled(features::kDefaultBrowserPromptRefresh));
    return {IconType::DEFAULT_BROWSER_PROMPT, Severity::LOW,
            features::kAppMenuChipColorPrimary.Get()};
  }
#endif
  return {IconType::NONE, Severity::NONE};
}

void AppMenuIconController::OnGlobalErrorsChanged() {
  UpdateDelegate();
}

void AppMenuIconController::OnUpgradeRecommended() {
  UpdateDelegate();
}

void AppMenuIconController::OnShowAppMenuPromptChanged() {
  UpdateDelegate();
}
