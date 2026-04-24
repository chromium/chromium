// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/check_op.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
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
        return AppMenuIconController::Severity::kLow;
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::kHigh;
    }
  } else {
    switch (level) {
      case UpgradeDetector::UPGRADE_ANNOYANCE_NONE:
        break;
      case UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW:
        // kVeryLow is meaningless for stable channels.
        return AppMenuIconController::Severity::kNone;
      case UpgradeDetector::UPGRADE_ANNOYANCE_LOW:
        return AppMenuIconController::Severity::kLow;
      case UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED:
        return AppMenuIconController::Severity::kMedium;
      case UpgradeDetector::UPGRADE_ANNOYANCE_GRACE:
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::kHigh;
    }
  }
  DCHECK_EQ(level, UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  return AppMenuIconController::Severity::kNone;
}

// Returns the app menu icon severity for a Global Error.
AppMenuIconController::Severity SeverityFromError(GlobalError* error) {
  CHECK(error);

  switch (error->GetSeverity()) {
    case GlobalError::SEVERITY_LOW:
      return AppMenuIconController::Severity::kLow;
    case GlobalError::SEVERITY_MEDIUM:
      return AppMenuIconController::Severity::kMedium;
    case GlobalError::SEVERITY_HIGH:
      return AppMenuIconController::Severity::kHigh;
  }
  NOTREACHED();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
#if !BUILDFLAG(IS_CHROMEOS)
  if (browser_defaults::kShowUpgradeMenuItem &&
      upgrade_detector_->notify_upgrade()) {
    UpgradeDetector::UpgradeNotificationAnnoyanceLevel level =
        upgrade_detector_->upgrade_notification_stage();
    // The severity may be NONE even if the detector has been notified of an
    // update. This can happen for beta and stable channels once the VERY_LOW
    // annoyance level is reached.
    auto severity = SeverityFromUpgradeLevel(is_unstable_channel_, level);
    if (severity != Severity::kNone) {
      return {IconType::kUpgradeNotification, severity};
    }
  }

  // If you change the severity here, make sure to also change the menu icon
  // and the bubble icon.
  if (auto* error = GlobalErrorServiceFactory::GetForProfile(profile_)
                        ->GetHighestSeverityGlobalErrorWithAppMenuItem()) {
    return {IconType::kGlobalError, SeverityFromError(error)};
  }
#endif

  return {IconType::kNone, Severity::kNone};
}

void AppMenuIconController::OnGlobalErrorsChanged() {
  UpdateDelegate();
}

void AppMenuIconController::OnUpgradeRecommended() {
  UpdateDelegate();
}

// static
std::u16string AppMenuIconController::GetIconLabel(IconType type,
                                                   Severity severity) {
  if (severity == Severity::kNone) {
    return std::u16string();
  } else if (type == IconType::kUpgradeNotification) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
    int message_id = IDS_APP_MENU_BUTTON_UPDATE;
    // Select an update text option randomly. Show this text in all browser
    // windows.
    static const int update_text_option = base::RandIntInclusive(1, 3);
    if (update_text_option == 1) {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT1;
    } else if (update_text_option == 2) {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT2;
    } else {
      message_id = IDS_APP_MENU_BUTTON_UPDATE_ALT3;
    }
    return l10n_util::GetStringUTF16(message_id);
#else
    return l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_UPDATE);
#endif
  } else {
    const int text_id = severity == Severity::kLow
                            ? IDS_APP_MENU_BUTTON_ACTION_REQUIRED
                            : IDS_APP_MENU_BUTTON_ERROR;
    return l10n_util::GetStringUTF16(text_id);
  }
}

// static
std::u16string AppMenuIconController::GetIconAccessibleName(IconType type) {
  std::u16string accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (type == IconType::kUpgradeNotification) {
    return l10n_util::GetStringFUTF16(IDS_ACCNAME_APP_UPGRADE_RECOMMENDED,
                                      accname_app);
  }
  return accname_app;
}

// static
std::u16string AppMenuIconController::GetIconTooltip(IconType type,
                                                     Severity severity) {
  if (severity == Severity::kNone) {
    return l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP);
  } else if (type == IconType::kUpgradeNotification) {
    return l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE);
  } else {
    return l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP_ALERT);
  }
}
