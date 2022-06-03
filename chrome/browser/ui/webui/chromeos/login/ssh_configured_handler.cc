// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/ssh_configured_handler.h"

#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/js_calls_container.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/login/localized_values_builder.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace chromeos {

namespace {

void OnDebugServiceAvailable(
    DebugDaemonClient::QueryDevFeaturesCallback callback,
    bool service_is_available) {
  if (!service_is_available) {
    std::move(callback).Run(/*succeeded=*/false,
                            debugd::DevFeatureFlag::DEV_FEATURES_DISABLED);
    return;
  }
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->QueryDebuggingFeatures(std::move(callback));
}

void QueryDebuggingFeatures(
    DebugDaemonClient::QueryDevFeaturesCallback callback) {
  chromeos::DebugDaemonClient* client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  client->WaitForServiceToBeAvailable(
      base::BindOnce(&OnDebugServiceAvailable, std::move(callback)));
}

}  // namespace

SshConfiguredHandler::SshConfiguredHandler(JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container) {}

SshConfiguredHandler::~SshConfiguredHandler() = default;

void SshConfiguredHandler::DeclareJSCallbacks() {
  AddCallback("getIsSshConfigured",
              &SshConfiguredHandler::HandleGetIsSshConfigured);
}

void SshConfiguredHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("sshWarningLogin", IDS_LOGIN_SSH_WARNING);
}

void SshConfiguredHandler::Initialize() {}

void SshConfiguredHandler::HandleGetIsSshConfigured(
    const std::string& callback_id) {
  AllowJavascript();
  callback_ids_.push_back(callback_id);

  if (is_ssh_configured_.has_value()) {
    ResolveCallbacks();
    return;
  }

  if (weak_factory_.HasWeakPtrs()) {
    // Pending request.
    return;
  }

  // Query for the debugging features.
  QueryDebuggingFeatures(
      base::BindOnce(&SshConfiguredHandler::OnGetDebuggingFeatures,
                     weak_factory_.GetWeakPtr()));
}

void SshConfiguredHandler::OnGetDebuggingFeatures(bool succeeded,
                                                  int feature_mask) {
  is_ssh_configured_ =
      succeeded && (feature_mask &
                    debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED);
  if (!IsJavascriptAllowed())
    return;

  ResolveCallbacks();
}

void SshConfiguredHandler::ResolveCallbacks() {
  DCHECK(is_ssh_configured_.has_value());
  for (const std::string& callback_id : callback_ids_) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(is_ssh_configured_.value()));
  }
  callback_ids_.clear();
}

}  // namespace chromeos
