// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/update_required_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

constexpr StaticOobeScreenId UpdateRequiredView::kScreenId;

UpdateRequiredScreenHandler::UpdateRequiredScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.UpdateRequiredScreen.userActed");
}

UpdateRequiredScreenHandler::~UpdateRequiredScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void UpdateRequiredScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("updateRequiredMessage",
               IDS_UPDATE_REQUIRED_LOGIN_SCREEN_MESSAGE);
  builder->Add("errorMessage",
               IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_INTERNAL_ERROR);
  builder->Add("eolMessage",
               ui::SubstituteChromeOSDeviceType(IDS_EOL_NOTIFICATION_EOL));
  builder->Add("selectNetworkButtonCaption", IDS_APP_START_CONFIGURE_NETWORK);
  builder->Add("updateButtonCaption",
               IDS_SETTINGS_ABOUT_PAGE_CHECK_FOR_UPDATES);
  builder->Add("rebootNeededMessage", IDS_UPDATE_COMPLETED);

  builder->Add("checkingForUpdatesTitle", IDS_CHECKING_FOR_UPDATES);
  builder->Add("updatingTitle", IDS_UPDATING_SCREEN_TITLE);

  builder->Add("downloading", IDS_DOWNLOADING);
  builder->Add("downloadingTimeLeftLong", IDS_DOWNLOADING_TIME_LEFT_LONG);
  builder->Add("downloadingTimeLeftStatusOneHour",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR);
  builder->Add("downloadingTimeLeftStatusMinutes",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_MINUTES);
  builder->Add("downloadingTimeLeftSmall", IDS_DOWNLOADING_TIME_LEFT_SMALL);

  builder->Add(
      "updateOverCellularPromptTitle",
      ui::SubstituteChromeOSDeviceType(IDS_UPDATE_OVER_CELLULAR_PROMPT_TITLE));
  builder->Add("updateOverCellularPromptMessage",
               IDS_UPDATE_OVER_CELLULAR_PROMPT_MESSAGE);
  builder->Add("AcceptUpdateOverCellularButton",
               IDS_OFFERS_CONSENT_INFOBAR_ENABLE_BUTTON);
  builder->Add("RejectUpdateOverCellularButton",
               IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON);
}

void UpdateRequiredScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void UpdateRequiredScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void UpdateRequiredScreenHandler::Hide() {}

void UpdateRequiredScreenHandler::Bind(UpdateRequiredScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void UpdateRequiredScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void UpdateRequiredScreenHandler::SetIsConnected(bool connected) {
  CallJS("login.UpdateRequiredScreen.setIsConnected", connected);
}

void UpdateRequiredScreenHandler::SetUpdateProgressUnavailable(
    bool unavailable) {
  CallJS("login.UpdateRequiredScreen.setUpdateProgressUnavailable",
         unavailable);
}

void UpdateRequiredScreenHandler::SetUpdateProgressValue(int progress) {
  CallJS("login.UpdateRequiredScreen.setUpdateProgressValue", progress);
}

void UpdateRequiredScreenHandler::SetUpdateProgressMessage(
    const base::string16& message) {
  CallJS("login.UpdateRequiredScreen.setUpdateProgressMessage", message);
}

void UpdateRequiredScreenHandler::SetEstimatedTimeLeftVisible(bool visible) {
  CallJS("login.UpdateRequiredScreen.setEstimatedTimeLeftVisible", visible);
}

void UpdateRequiredScreenHandler::SetEstimatedTimeLeft(int seconds_left) {
  CallJS("login.UpdateRequiredScreen.setEstimatedTimeLeft", seconds_left);
}

void UpdateRequiredScreenHandler::SetUIState(
    UpdateRequiredView::UIState ui_state) {
  CallJS("login.UpdateRequiredScreen.setUIState", static_cast<int>(ui_state));
}

}  // namespace chromeos
