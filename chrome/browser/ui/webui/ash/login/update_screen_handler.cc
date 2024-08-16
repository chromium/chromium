// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

namespace {

constexpr bool strings_equal(char const* a, char const* b) {
  return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}
static_assert(
    strings_equal(UpdateView::kScreenId.name,
                  "oobe"
                  // break with comment is used here so the verification value
                  // won't get automatically renamed by mass renaming tools
                  "-update"),
    "The update screen id must never change");

// These values must be kept in sync with UIState in JS code.
constexpr const char kCheckingForUpdate[] = "checking";
constexpr const char kUpdateInProgress[] = "update";
constexpr const char kRestartInProgress[] = "restart";
constexpr const char kManualReboot[] = "reboot";
constexpr const char kCellularPermission[] = "cellular";
constexpr const char kOptOutInfo[] = "opt-out-info";

}  // namespace

UpdateScreenHandler::UpdateScreenHandler() : BaseScreenHandler(kScreenId) {}

UpdateScreenHandler::~UpdateScreenHandler() = default;

void UpdateScreenHandler::Show(bool is_opt_out_enabled) {
  base::Value::Dict data;
  data.Set("isOptOutEnabled", is_opt_out_enabled);
  ShowInWebUI(std::move(data));
}

void UpdateScreenHandler::SetUpdateState(UpdateView::UIState value) {
  switch (value) {
    case UpdateView::UIState::kCheckingForUpdate:
      CallExternalAPI("setUpdateState", kCheckingForUpdate);
      break;
    case UpdateView::UIState::kUpdateInProgress:
      CallExternalAPI("setUpdateState", kUpdateInProgress);
      break;
    case UpdateView::UIState::kRestartInProgress:
      CallExternalAPI("setUpdateState", kRestartInProgress);
      break;
    case UpdateView::UIState::kManualReboot:
      CallExternalAPI("setUpdateState", kManualReboot);
      break;
    case UpdateView::UIState::kCellularPermission:
      CallExternalAPI("setUpdateState", kCellularPermission);
      break;
    case UpdateView::UIState::kOptOutInfo:
      CallExternalAPI("setUpdateState", kOptOutInfo);
      break;
  }
}

void UpdateScreenHandler::SetUpdateStatus(
    int percent,
    const std::u16string& percent_message,
    const std::u16string& timeleft_message) {
  CallExternalAPI("setUpdateStatus", percent, percent_message,
                  timeleft_message);
}

void UpdateScreenHandler::ShowLowBatteryWarningMessage(bool value) {
  CallExternalAPI("showLowBatteryWarningMessage", value);
}

void UpdateScreenHandler::SetAutoTransition(bool value) {
  CallExternalAPI("setAutoTransition", value);
}

void UpdateScreenHandler::SetCancelUpdateShortcutEnabled(bool value) {
  CallExternalAPI("setCancelUpdateShortcutEnabled", value);
}

base::WeakPtr<UpdateView> UpdateScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("updateCompeletedMsg", IDS_UPDATE_COMPLETED);
  builder->Add("updateCompeletedRebootingMsg", IDS_UPDATE_COMPLETED_REBOOTING);
  builder->Add("updateStatusTitle", IDS_UPDATE_STATUS_TITLE);
  builder->Add("updateScreenAccessibleTitle",
               IDS_UPDATE_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkingForUpdates", IDS_CHECKING_FOR_UPDATES);

  builder->Add("slideUpdateAdditionalSettingsTitle",
               IDS_UPDATE_SLIDE_UPDATE_ADDITIONAL_SETTINGS_TITLE);
  builder->Add("slideUpdateAdditionalSettingsText",
               IDS_UPDATE_SLIDE_UPDATE_ADDITIONAL_SETTINGS_TEXT);
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
  builder->Add("noUpdateAvailableTitle", IDS_UPDATE_NO_UPDATE_AVAILABLE_TITLE);
  builder->Add("noUpdateAvailableText", IDS_UPDATE_NO_UPDATE_AVAILABLE_TEXT);
  builder->Add("slideLabel", IDS_UPDATE_SLIDE_LABEL);
  builder->Add("slideSelectedButtonLabel", IDS_UPDATE_SELECTED_BUTTON_LABEL);
  builder->Add("slideUnselectedButtonLabel",
               IDS_UPDATE_UNSELECTED_BUTTON_LABEL);

  builder->Add("gettingDeviceReadyTitle", IDS_GETTING_DEVICE_READY);

  builder->Add("updateOverCellularPromptTitle",
               IDS_UPDATE_OVER_CELLULAR_PROMPT_TITLE);
  builder->Add("updateOverCellularPromptMessage",
               IDS_UPDATE_OVER_CELLULAR_PROMPT_MESSAGE);
}

}  // namespace ash
