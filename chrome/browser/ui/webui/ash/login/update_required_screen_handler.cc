// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"

#include <memory>

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/update_required_screen.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

UpdateRequiredScreenHandler::UpdateRequiredScreenHandler()
    : BaseScreenHandler(kScreenId) {}

UpdateRequiredScreenHandler::~UpdateRequiredScreenHandler() = default;

void UpdateRequiredScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("updateRequiredTitle", IDS_UPDATE_REQUIRED_SCREEN_TITLE);
  builder->Add("updateRequiredMessage", IDS_UPDATE_REQUIRED_SCREEN_MESSAGE);
  builder->Add("errorMessage",
               IDS_BROWSER_SHARING_ERROR_DIALOG_TEXT_INTERNAL_ERROR);
  builder->Add("eolTitle", IDS_UPDATE_REQUIRED_SCREEN_EOL_TITLE);
  builder->Add("eolMessage", IDS_UPDATE_REQUIRED_SCREEN_EOL_MESSAGE);
  builder->Add("selectNetworkButtonCaption",
               IDS_UPDATE_REQUIRED_SCREEN_OPEN_NETWORK_SETTINGS);
  builder->Add("updateButtonCaption", IDS_UPDATE_REQUIRED_SCREEN_START_UPDATE);
  builder->Add("rebootNeededMessage", IDS_UPDATE_COMPLETED);
  builder->Add("checkingForUpdatesTitle", IDS_CHECKING_FOR_UPDATES);
  builder->Add("updatingTitle", IDS_UPDATING_SCREEN_TITLE);
  builder->Add("updatingMessage", IDS_UPDATE_REQUIRED_UPDATING_MESSAGE);
  builder->AddF("updatingMessage", IDS_UPDATE_REQUIRED_UPDATING_MESSAGE,
                ui::GetChromeOSDeviceName());
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
  builder->Add("updateOverMeteredNetworkMessage",
               IDS_UPDATE_REQUIRED_SCREEN_METERED_MESSAGE);
  builder->Add("AcceptUpdateOverCellularButton",
               IDS_UPDATE_REQUIRED_SCREEN_ALLOW_METERED);
  builder->Add("RejectUpdateOverCellularButton",
               IDS_OFFERS_CONSENT_INFOBAR_DISABLE_BUTTON);
  builder->Add("noNetworkMessage",
               IDS_UPDATE_REQUIRED_SCREEN_NO_NETWORK_MESSAGE);
  builder->Add("eolAdminMessageTitle", IDS_UPDATE_REQUIRED_EOL_ADMIN_MESSAGE);
  builder->Add("eolDeleteUsersDataMessage",
               IDS_UPDATE_REQUIRED_EOL_DELETE_USERS_DATA_MESSAGE);
  builder->Add("eolNoUsersDataMessage",
               IDS_UPDATE_REQUIRED_EOL_NO_USERS_DATA_MESSAGE);
  builder->Add("eolDeleteUsersDataPopupMessage",
               IDS_UPDATE_REQUIRED_EOL_DELETE_USERS_DATA_POPUP_MESSAGE);
  builder->Add("eolDeleteUsersDataPopupTitle",
               IDS_UPDATE_REQUIRED_EOL_DELETE_USERS_DATA_POPUP_TITLE);
  builder->Add("eolDeleteUsersDataConfirm",
               IDS_UPDATE_REQUIRED_EOL_DELETE_USERS_DATA_CONFIRM);
  builder->Add("eolDeleteUsersDataCancel",
               IDS_UPDATE_REQUIRED_EOL_DELETE_USERS_DATA_CANCEL);

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  builder->Add("cancelUpdateHint", IDS_UPDATE_CANCEL);
  builder->Add("cancelledUpdateMessage", IDS_UPDATE_CANCELLED);
#else
  builder->Add("cancelUpdateHint", IDS_EMPTY_STRING);
  builder->Add("cancelledUpdateMessage", IDS_EMPTY_STRING);
#endif
}

void UpdateRequiredScreenHandler::SetEnterpriseAndDeviceName(
    const std::string& enterpriseDomain,
    const std::u16string& deviceName) {
  CallExternalAPI("setEnterpriseAndDeviceName", enterpriseDomain, deviceName);
}

void UpdateRequiredScreenHandler::SetEolMessage(const std::string& eolMessage) {
  CallExternalAPI("setEolMessage", eolMessage);
}

void UpdateRequiredScreenHandler::Show() {
  ShowInWebUI();
}

void UpdateRequiredScreenHandler::SetIsConnected(bool connected) {
  CallExternalAPI("setIsConnected", connected);
}

void UpdateRequiredScreenHandler::SetUpdateProgressUnavailable(
    bool unavailable) {
  CallExternalAPI("setUpdateProgressUnavailable", unavailable);
}

void UpdateRequiredScreenHandler::SetUpdateProgressValue(int progress) {
  CallExternalAPI("setUpdateProgressValue", progress);
}

void UpdateRequiredScreenHandler::SetUpdateProgressMessage(
    const std::u16string& message) {
  CallExternalAPI("setUpdateProgressMessage", message);
}

void UpdateRequiredScreenHandler::SetEstimatedTimeLeftVisible(bool visible) {
  CallExternalAPI("setEstimatedTimeLeftVisible", visible);
}

void UpdateRequiredScreenHandler::SetEstimatedTimeLeft(int seconds_left) {
  CallExternalAPI("setEstimatedTimeLeft", seconds_left);
}

void UpdateRequiredScreenHandler::SetUIState(
    UpdateRequiredView::UIState ui_state) {
  CallExternalAPI("setUIState", static_cast<int>(ui_state));
}

void UpdateRequiredScreenHandler::SetIsUserDataPresent(bool data_present) {
  CallExternalAPI("setIsUserDataPresent", data_present);
}

base::WeakPtr<UpdateRequiredView> UpdateRequiredScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
