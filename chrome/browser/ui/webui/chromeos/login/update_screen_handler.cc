// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include <memory>

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

constexpr StaticOobeScreenId UpdateView::kScreenId;

UpdateScreenHandler::UpdateScreenHandler(JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.UpdateScreen.userActed");
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  CHECK(accessibility_manager);
  accessibility_subscription_ = accessibility_manager->RegisterCallback(
      base::Bind(&UpdateScreenHandler::OnAccessibilityStatusChanged,
                 base::Unretained(this)));
}

UpdateScreenHandler::~UpdateScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void UpdateScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void UpdateScreenHandler::Hide() {}

void UpdateScreenHandler::Bind(UpdateScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void UpdateScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void UpdateScreenHandler::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type == ACCESSIBILITY_MANAGER_SHUTDOWN) {
    accessibility_subscription_.reset();
    return;
  }

  CallJS("login.UpdateScreen.setAutoTransition",
         !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
}

void UpdateScreenHandler::SetUIState(UpdateView::UIState value) {
  CallJS("login.UpdateScreen.setUIState", static_cast<int>(value));
}

void UpdateScreenHandler::SetUpdateStatus(
    int percent,
    const base::string16& percent_message,
    const base::string16& timeleft_message) {
  CallJS("login.UpdateScreen.setUpdateStatus", percent, percent_message,
         timeleft_message);
}

void UpdateScreenHandler::SetEstimatedTimeLeft(int value) {
  CallJS("login.UpdateScreen.setEstimatedTimeLeft", value);
}

void UpdateScreenHandler::SetShowEstimatedTimeLeft(bool value) {
  CallJS("login.UpdateScreen.showEstimatedTimeLeft", value);
}

void UpdateScreenHandler::SetUpdateCompleted(bool value) {
  CallJS("login.UpdateScreen.setUpdateCompleted", value);
}

void UpdateScreenHandler::SetShowCurtain(bool value) {
  CallJS("login.UpdateScreen.showUpdateCurtain", value);
}

void UpdateScreenHandler::SetProgressMessage(const base::string16& value) {
  CallJS("login.UpdateScreen.setProgressMessage", value);
}

void UpdateScreenHandler::SetProgress(int value) {
  CallJS("login.UpdateScreen.setUpdateProgress", value);
}

void UpdateScreenHandler::SetRequiresPermissionForCellular(bool value) {
  CallJS("login.UpdateScreen.setRequiresPermissionForCellular", value);
}

void UpdateScreenHandler::SetCancelUpdateShortcutEnabled(bool value) {
  CallJS("login.UpdateScreen.setCancelUpdateShortcutEnabled", value);
}

void UpdateScreenHandler::ShowLowBatteryWarningMessage(bool value) {
  CallJS("login.UpdateScreen.showLowBatteryWarningMessage", value);
}

void UpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("checkingForUpdatesMsg", IDS_CHECKING_FOR_UPDATE_MSG);
  builder->AddF("installingUpdateDesc", IDS_UPDATE_MSG,
                ui::GetChromeOSDeviceName());
  builder->Add("updateCompeletedMsg", IDS_UPDATE_COMPLETED);
  builder->Add("updateCompeletedRebootingMsg", IDS_UPDATE_COMPLETED_REBOOTING);
  builder->Add("updateStatusTitle", IDS_UPDATE_STATUS_TITLE);
  builder->Add("updateScreenAccessibleTitle",
               IDS_UPDATE_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkingForUpdates", IDS_CHECKING_FOR_UPDATES);
  builder->Add("downloading", IDS_DOWNLOADING);
  builder->Add("downloadingTimeLeftLong", IDS_DOWNLOADING_TIME_LEFT_LONG);
  builder->Add("downloadingTimeLeftStatusOneHour",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR);
  builder->Add("downloadingTimeLeftStatusMinutes",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_MINUTES);
  builder->Add("downloadingTimeLeftSmall", IDS_DOWNLOADING_TIME_LEFT_SMALL);

  builder->Add("slideUpdateTitle", IDS_UPDATE_SLIDE_UPDATE_TITLE);
  builder->Add("slideUpdateText", IDS_UPDATE_SLIDE_UPDATE_TEXT);
  builder->Add("slideAntivirusTitle", IDS_UPDATE_SLIDE_ANTIVIRUS_TITLE);
  builder->Add("slideAntivirusText", IDS_UPDATE_SLIDE_ANTIVIRUS_TEXT);
  builder->Add("slideAppsTitle", IDS_UPDATE_SLIDE_APPS_TITLE);
  builder->Add("slideAppsText", IDS_UPDATE_SLIDE_APPS_TEXT);
  builder->Add("slideAccountTitle", IDS_UPDATE_SLIDE_ACCOUNT_TITLE);
  builder->Add("slideAccountText", IDS_UPDATE_SLIDE_ACCOUNT_TEXT);
  builder->Add("batteryWarningTitle", IDS_UPDATE_BATTERY_WARNING_TITLE);
  builder->Add("batteryWarningText", IDS_UPDATE_BATTERY_WARNING_TEXT);

  builder->Add("slideLabel", IDS_UPDATE_SLIDE_LABEL);
  builder->Add("slideSelectedButtonLabel", IDS_UPDATE_SELECTED_BUTTON_LABEL);
  builder->Add("slideUnselectedButtonLabel",
               IDS_UPDATE_UNSELECTED_BUTTON_LABEL);

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  builder->Add("cancelUpdateHint", IDS_UPDATE_CANCEL);
  builder->Add("cancelledUpdateMessage", IDS_UPDATE_CANCELLED);
#else
  builder->Add("cancelUpdateHint", IDS_EMPTY_STRING);
  builder->Add("cancelledUpdateMessage", IDS_EMPTY_STRING);
#endif

  builder->Add("updateOverCellularPromptTitle",
               IDS_UPDATE_OVER_CELLULAR_PROMPT_TITLE);
  builder->Add("updateOverCellularPromptMessage",
               IDS_UPDATE_OVER_CELLULAR_PROMPT_MESSAGE);

  // For Material Design OOBE
  builder->Add("updatingScreenTitle", IDS_UPDATING_SCREEN_TITLE);
}

void UpdateScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetBoolKey("betterUpdateScreenFeatureEnabled",
                   chromeos::features::IsBetterUpdateEnabled());
  BaseScreenHandler::GetAdditionalParameters(dict);
}

void UpdateScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
