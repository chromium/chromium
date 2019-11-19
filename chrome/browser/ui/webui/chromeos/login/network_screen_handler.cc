// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"

#include <stddef.h>

#include "base/values.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/ui/webui/chromeos/login/core_oobe_handler.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId NetworkScreenView::kScreenId;

NetworkScreenHandler::NetworkScreenHandler(JSCallsContainer* js_calls_container,
                                           CoreOobeView* core_oobe_view)
    : BaseScreenHandler(kScreenId, js_calls_container),
      core_oobe_view_(core_oobe_view) {
  set_user_acted_method_path("login.NetworkScreen.userActed");
  DCHECK(core_oobe_view_);
}

NetworkScreenHandler::~NetworkScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void NetworkScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  // In OOBE all physical network technologies should be enabled, so the user is
  // able to select any of the available networks on the device. Enabled
  // technologies should not be changed if network screen is shown outside of
  // OOBE.
  // If OOBE is not completed, we assume that the only instance of this object
  // could be OOBE itself.
  if (!StartupUtils::IsOobeCompleted()) {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::Physical(), true,
                                  chromeos::network_handler::ErrorCallback());
  }

  base::DictionaryValue data;
  data.SetBoolean("isDemoModeSetup",
                  DemoSetupController::IsOobeDemoSetupFlowInProgress());
  ShowScreenWithData(kScreenId, &data);
}

void NetworkScreenHandler::Hide() {}

void NetworkScreenHandler::Bind(NetworkScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void NetworkScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void NetworkScreenHandler::ShowError(const base::string16& message) {
  CallJS("login.NetworkScreen.showError", message);
}

void NetworkScreenHandler::ClearErrors() {
  if (page_is_ready())
    core_oobe_view_->ClearErrors();
}

void NetworkScreenHandler::ShowConnectingStatus(
    bool connecting,
    const base::string16& network_id) {}

void NetworkScreenHandler::SetOfflineDemoModeEnabled(bool enabled) {
  CallJS("login.NetworkScreen.setOfflineDemoModeEnabled", enabled);
}

void NetworkScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("networkSectionTitle", IDS_NETWORK_SELECTION_TITLE);
  builder->Add("networkSectionHint", IDS_NETWORK_SELECTION_HINT);
  builder->Add("proxySettingsListItemName",
               IDS_NETWORK_PROXY_SETTINGS_LIST_ITEM_NAME);
  builder->Add("addWiFiListItemName", IDS_NETWORK_ADD_WI_FI_LIST_ITEM_NAME);
  builder->Add("offlineDemoSetupListItemName",
               IDS_NETWORK_OFFLINE_DEMO_SETUP_LIST_ITEM_NAME);
  network_element::AddLocalizedValuesToBuilder(builder);
}

void NetworkScreenHandler::GetAdditionalParameters(
    base::DictionaryValue* dict) {
}

void NetworkScreenHandler::Initialize() {
  if (show_on_init_) {
    show_on_init_ = false;
    Show();
  }
}

}  // namespace chromeos
