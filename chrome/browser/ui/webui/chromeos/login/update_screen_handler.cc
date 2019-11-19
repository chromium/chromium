// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/update_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

constexpr StaticOobeScreenId UpdateView::kScreenId;

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

void UpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("checkingForUpdatesMsg", IDS_CHECKING_FOR_UPDATE_MSG);
  builder->AddF("installingUpdateDesc", IDS_UPDATE_MSG,
                ui::GetChromeOSDeviceName());
  builder->Add("updateCompeletedMsg", IDS_UPDATE_COMPLETED);
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

#if !defined(OFFICIAL_BUILD)
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

void UpdateScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
