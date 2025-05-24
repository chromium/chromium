// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_handler.h"

#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace {
constexpr char kInitialize[] = "initialize";
constexpr char kAddNetwork[] = "addNetwork";
constexpr char kShowNetworkDetails[] = "showNetworkDetails";
constexpr char kShowNetworkConfig[] = "showNetworkConfig";
constexpr char kGetHostname[] = "getHostname";

const char kFloatingWorkspaceDialog[] = "$(\'floating-workspace-dialog\').";
}  // namespace

FloatingWorkspaceDialogHandler::FloatingWorkspaceDialogHandler() = default;

FloatingWorkspaceDialogHandler::~FloatingWorkspaceDialogHandler() = default;

void FloatingWorkspaceDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kInitialize,
      base::BindRepeating(&FloatingWorkspaceDialogHandler::Initialize,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kAddNetwork,
      base::BindRepeating(&FloatingWorkspaceDialogHandler::AddNetwork,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kShowNetworkDetails,
      base::BindRepeating(&FloatingWorkspaceDialogHandler::ShowNetworkDetails,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kShowNetworkConfig,
      base::BindRepeating(&FloatingWorkspaceDialogHandler::ShowNetworkConfig,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      kGetHostname,
      base::BindRepeating(&FloatingWorkspaceDialogHandler::GetHostname,
                          weak_ptr_factory_.GetWeakPtr()));
}

void FloatingWorkspaceDialogHandler::Initialize(const base::Value::List& args) {
  AllowJavascript();
  switch (state_) {
    case FloatingWorkspaceDialog::State::kDefault:
      CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                             "showDefaultScreen");
      break;
    case FloatingWorkspaceDialog::State::kNetwork:
      CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                             "showNetworkScreen");
      break;
    case FloatingWorkspaceDialog::State::kError:
      CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                             "showErrorScreen");
      break;
  }
}

void FloatingWorkspaceDialogHandler::ShowDefaultScreen() {
  if (state_ != FloatingWorkspaceDialog::State::kDefault &&
      IsJavascriptAllowed()) {
    CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                           "showDefaultScreen");
  }
  state_ = FloatingWorkspaceDialog::State::kDefault;
}

void FloatingWorkspaceDialogHandler::ShowNetworkScreen() {
  if (state_ != FloatingWorkspaceDialog::State::kNetwork &&
      IsJavascriptAllowed()) {
    CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                           "showNetworkScreen");
  }
  state_ = FloatingWorkspaceDialog::State::kNetwork;
}

void FloatingWorkspaceDialogHandler::ShowErrorScreen() {
  if (state_ != FloatingWorkspaceDialog::State::kError &&
      IsJavascriptAllowed()) {
    CallJavascriptFunction(std::string(kFloatingWorkspaceDialog) +
                           "showErrorScreen");
  }
  state_ = FloatingWorkspaceDialog::State::kError;
}

void FloatingWorkspaceDialogHandler::ShowNetworkDetails(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string guid = args[0].GetString();

  // We need to pass NativeWindow to the network dialog here, because otherwise
  // the network dialog would be shown behind our main modal dialog.
  auto dialog = ash::FloatingWorkspaceDialog::GetNativeWindow();
  if (dialog) {
    InternetDetailDialog::ShowDialog(guid, dialog);
  }
}

void FloatingWorkspaceDialogHandler::ShowNetworkConfig(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string guid = args[0].GetString();

  // We need to pass NativeWindow to the network dialog here, because otherwise
  // the network dialog would be shown behind our main modal dialog.
  auto dialog = ash::FloatingWorkspaceDialog::GetNativeWindow();
  if (dialog) {
    InternetConfigDialog::ShowDialogForNetworkId(guid, dialog);
  }
}

void FloatingWorkspaceDialogHandler::AddNetwork(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string onc_type = args[0].GetString();

  // We need to pass NativeWindow to the network dialog here, because otherwise
  // the network dialog would be shown behind our main modal dialog.
  auto dialog = ash::FloatingWorkspaceDialog::GetNativeWindow();
  if (dialog) {
    InternetConfigDialog::ShowDialogForNetworkType(onc_type, dialog);
  }
}

// This is needed for proxy connection.
void FloatingWorkspaceDialogHandler::GetHostname(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();
  std::string hostname =
      NetworkHandler::Get()->network_state_handler()->hostname();

  ResolveJavascriptCallback(callback_id, hostname);
}

}  // namespace ash
