// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_reauth_dialogs.h"
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
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash {

namespace {

constexpr char kInitialize[] = "initialize";
constexpr char kAddNetwork[] = "addNetwork";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";
constexpr char kGetHostname[] = "getHostname";

}  // namespace

NetworkConfigMessageHandler::NetworkConfigMessageHandler() {}

NetworkConfigMessageHandler::~NetworkConfigMessageHandler() = default;

void NetworkConfigMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInitialize, base::BindRepeating(&NetworkConfigMessageHandler::Initialize,
                                       weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kAddNetwork, base::BindRepeating(&NetworkConfigMessageHandler::AddNetwork,
                                       weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kShowNetworkDetails,
      base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkDetails,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kShowNetworkConfig,
      base::BindRepeating(&NetworkConfigMessageHandler::ShowNetworkConfig,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kGetHostname,
      base::BindRepeating(&NetworkConfigMessageHandler::GetHostname,
                          weak_ptr_factory_.GetWeakPtr()));
}

void NetworkConfigMessageHandler::Initialize(const base::Value::List& args) {
  AllowJavascript();

  // Check if the main dialog exists and notify that the network dialog has
  // been loaded.
  LockScreenStartReauthDialog* start_reauth_dialog =
      LockScreenStartReauthDialog::GetInstance();
  if (!start_reauth_dialog)
    return;
  start_reauth_dialog->OnNetworkDialogReadyForTesting();
}

void NetworkConfigMessageHandler::ShowNetworkDetails(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string guid = args[0].GetString();

  InternetDetailDialog::ShowDialog(guid);
}

void NetworkConfigMessageHandler::ShowNetworkConfig(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string guid = args[0].GetString();

  InternetConfigDialog::ShowDialogForNetworkId(guid);
}

void NetworkConfigMessageHandler::AddNetwork(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string onc_type = args[0].GetString();

  InternetConfigDialog::ShowDialogForNetworkType(onc_type);
}

void NetworkConfigMessageHandler::GetHostname(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();
  std::string hostname =
      NetworkHandler::Get()->network_state_handler()->hostname();
  Respond(callback_id, hostname);
}

void NetworkConfigMessageHandler::Respond(const std::string& callback_id,
                                          base::ValueView response) {
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, response);
}

}  // namespace ash
