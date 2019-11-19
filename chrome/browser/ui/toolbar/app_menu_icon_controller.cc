// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

namespace {

// Maps an upgrade level to a severity level. When |show_very_low_upgrade_level|
// is true, VERY_LOW through HIGH all return Severity::LOW. Otherwise, VERY_LOW
// is ignored and LOW through HIGH return their respective Severity level.
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
      case UpgradeDetector::UPGRADE_ANNOYANCE_HIGH:
      case UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL:
        return AppMenuIconController::Severity::HIGH;
    }
  }
  DCHECK_EQ(level, UpgradeDetector::UPGRADE_ANNOYANCE_NONE);

  return AppMenuIconController::Severity::NONE;
}

// Return true if the browser is updating on the dev or canary channels.
bool IsUnstableChannel() {
  // Unbranded (Chromium) builds are on the UNKNOWN channel, so check explicitly
  // for the Google Chrome channels that are considered "unstable". This ensures
  // that Chromium builds get the default behavior.
  const version_info::Channel channel = chrome::GetChannel();
  return channel == version_info::Channel::DEV ||
         channel == version_info::Channel::CANARY;
}

// Returns the icon color based on |severity|. |promo_highlight_color|, if
// specified, overrides the basic color when |severity| is NONE.
SkColor GetIconColorForSeverity(AppMenuIconController::Delegate* delegate,
                                AppMenuIconController::Severity severity,
                                base::Optional<SkColor> promo_highlight_color) {
  ui::NativeTheme::ColorId color_id =
      ui::NativeTheme::kColorId_AlertSeverityHigh;
  switch (severity) {
    case AppMenuIconController::Severity::NONE:
      if (promo_highlight_color)
        return promo_highlight_color.value();
      return delegate->GetViewThemeProvider()->GetColor(
          ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
    case AppMenuIconController::Severity::LOW:
      color_id = ui::NativeTheme::kColorId_AlertSeverityLow;
      break;
    case AppMenuIconController::Severity::MEDIUM:
      color_id = ui::NativeTheme::kColorId_AlertSeverityMedium;
      break;
    case AppMenuIconController::Severity::HIGH:
      break;
  }
  return delegate->GetViewNativeTheme()->GetSystemColor(color_id);
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

  global_error_observer_.Add(
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

  return {IconType::NONE, Severity::NONE};
}

gfx::ImageSkia AppMenuIconController::GetIconImage(
    bool touch_ui,
    base::Optional<SkColor> promo_highlight_color) const {
  const auto type_and_severity = GetTypeAndSeverity();
  const gfx::VectorIcon* icon_id =
      touch_ui ? &kBrowserToolsTouchIcon : &kBrowserToolsIcon;
  switch (type_and_severity.type) {
    case AppMenuIconController::IconType::NONE:
      break;
    case AppMenuIconController::IconType::UPGRADE_NOTIFICATION:
      icon_id =
          touch_ui ? &kBrowserToolsUpdateTouchIcon : &kBrowserToolsUpdateIcon;
      break;
    case AppMenuIconController::IconType::GLOBAL_ERROR:
      icon_id =
          touch_ui ? &kBrowserToolsErrorTouchIcon : &kBrowserToolsErrorIcon;
      break;
  }
  return gfx::CreateVectorIcon(
      *icon_id, GetIconColorForSeverity(delegate_, type_and_severity.severity,
                                        promo_highlight_color));
}

SkColor AppMenuIconController::GetIconColor(
    base::Optional<SkColor> promo_highlight_color) const {
  return GetIconColorForSeverity(delegate_, GetTypeAndSeverity().severity,
                                 promo_highlight_color);
}

void AppMenuIconController::OnGlobalErrorsChanged() {
  UpdateDelegate();
}

void AppMenuIconController::OnUpgradeRecommended() {
  UpdateDelegate();
}
