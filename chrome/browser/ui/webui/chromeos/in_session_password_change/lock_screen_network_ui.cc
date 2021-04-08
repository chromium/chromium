// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog_launcher.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

constexpr char kAddNetwork[] = "addNetwork";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler() {}
  NetworkConfigMessageHandler(NetworkConfigMessageHandler const&) = delete;
  NetworkConfigMessageHandler& operator=(const NetworkConfigMessageHandler&) =
      delete;
  ~NetworkConfigMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        kAddNetwork,
        base::BindRepeating(&NetworkConfigMessageHandler::AddNetwork,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkDetails,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkDetails,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        kShowNetworkConfig,
        base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkConfig,
                            base::Unretained(this)));
  }

 private:
  void ShowNetworkDetails(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string guid;
    CHECK(arg_list->GetString(0, &guid));

    InternetDetailDialog::ShowDialog(guid);
  }

  void ShowNetworkConfig(const base::ListValue* arg_list) {
    CHECK_EQ(1u, arg_list->GetSize());
    std::string guid;
    CHECK(arg_list->GetString(0, &guid));

    InternetConfigDialog::ShowDialogForNetworkId(guid);
  }

  void AddNetwork(const base::ListValue* args) {
    std::string onc_type;
    args->GetString(0, &onc_type);
    InternetConfigDialog::ShowDialogForNetworkType(onc_type);
  }

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_{this};
};

}  // namespace

// static
void LockScreenNetworkUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString(
      "titleText", l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings->SetString(
      "lockScreenNetworkTitle",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings->SetString(
      "lockScreenNetworkSubtitle",
      l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_NETWORK_SUBTITLE,
                                 ui::GetChromeOSDeviceName()));
  localized_strings->SetString(
      "lockScreenCancelButton",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CANCEL_BUTTON));
}

LockScreenNetworkUI::LockScreenNetworkUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<NetworkConfigMessageHandler>());

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUILockScreenNetworkHost);

  // TODO(crbug.com/1098690): Trusted Type Polymer.
  html->DisableTrustedTypesCSP();

  html->AddLocalizedStrings(localized_strings);

  network_element::AddLocalizedStrings(html);
  network_element::AddOncLocalizedStrings(html);
  html->UseStringsJs();

  html->AddResourcePath("lock_screen_network.js", IDR_LOCK_SCREEN_NETWORK_JS);
  html->SetDefaultResource(IDR_LOCK_SCREEN_NETWORK_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

LockScreenNetworkUI::~LockScreenNetworkUI() {}

void LockScreenNetworkUI::BindInterface(
    mojo::PendingReceiver<network_config::mojom::CrosNetworkConfig> receiver) {
  ash::GetNetworkConfigService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LockScreenNetworkUI)

}  // namespace chromeos
