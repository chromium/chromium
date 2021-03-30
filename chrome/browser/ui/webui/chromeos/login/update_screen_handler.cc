// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/update_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

constexpr StaticOobeScreenId UpdateView::kScreenId;

namespace {

constexpr bool strings_equal(char const* a, char const* b) {
  return *a == *b && (*a == '\0' || strings_equal(a + 1, b + 1));
}
static_assert(strings_equal(UpdateView::kScreenId.name, "oobe-update"),
              "The update screen id must never change");

// These values must be kept in sync with UIState in JS code.
constexpr const char kCheckingForUpdate[] = "checking";
constexpr const char kUpdateInProgress[] = "update";
constexpr const char kRestartInProgress[] = "restart";
constexpr const char kManualReboot[] = "reboot";
constexpr const char kCellularPermission[] = "cellular";

}  // namespace

UpdateScreenHandler::UpdateScreenHandler(JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.UpdateScreen.userActed");
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

void UpdateScreenHandler::SetUpdateState(UpdateView::UIState value) {
  switch (value) {
    case UpdateView::UIState::kCheckingForUpdate:
      CallJS("login.UpdateScreen.setUpdateState",
             std::string(kCheckingForUpdate));
      break;
    case UpdateView::UIState::kUpdateInProgress:
      CallJS("login.UpdateScreen.setUpdateState",
             std::string(kUpdateInProgress));
      break;
    case UpdateView::UIState::kRestartInProgress:
      CallJS("login.UpdateScreen.setUpdateState",
             std::string(kRestartInProgress));
      break;
    case UpdateView::UIState::kManualReboot:
      CallJS("login.UpdateScreen.setUpdateState", std::string(kManualReboot));
      break;
    case UpdateView::UIState::kCellularPermission:
      CallJS("login.UpdateScreen.setUpdateState",
             std::string(kCellularPermission));
      break;
  }
}

void UpdateScreenHandler::SetUpdateStatus(
    int percent,
    const std::u16string& percent_message,
    const std::u16string& timeleft_message) {
  CallJS("login.UpdateScreen.setUpdateStatus", percent, percent_message,
         timeleft_message);
}

void UpdateScreenHandler::ShowLowBatteryWarningMessage(bool value) {
  CallJS("login.UpdateScreen.showLowBatteryWarningMessage", value);
}

void UpdateScreenHandler::SetAutoTransition(bool value) {
  CallJS("login.UpdateScreen.setAutoTransition", value);
}

void UpdateScreenHandler::SetCancelUpdateShortcutEnabled(bool value) {
  CallJS("login.UpdateScreen.setCancelUpdateShortcutEnabled", value);
}

void UpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("updateCompeletedMsg", IDS_UPDATE_COMPLETED);
  builder->Add("updateCompeletedRebootingMsg", IDS_UPDATE_COMPLETED_REBOOTING);
  builder->Add("updateStatusTitle", IDS_UPDATE_STATUS_TITLE);
  builder->Add("updateScreenAccessibleTitle",
               IDS_UPDATE_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkingForUpdates", IDS_CHECKING_FOR_UPDATES);

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
}

void UpdateScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
