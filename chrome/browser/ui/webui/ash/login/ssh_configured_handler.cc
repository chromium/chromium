// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/ssh_configured_handler.h"

#include "chrome/browser/ui/webui/ash/login/base_webui_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/login/localized_values_builder.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"

namespace ash {

namespace {

void OnDebugServiceAvailable(
    DebugDaemonClient::QueryDevFeaturesCallback callback,
    bool service_is_available) {
  if (!service_is_available) {
    std::move(callback).Run(/*succeeded=*/false,
                            debugd::DevFeatureFlag::DEV_FEATURES_DISABLED);
    return;
  }
  DebugDaemonClient::Get()->QueryDebuggingFeatures(std::move(callback));
}

void QueryDebuggingFeatures(
    DebugDaemonClient::QueryDevFeaturesCallback callback) {
  DebugDaemonClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&OnDebugServiceAvailable, std::move(callback)));
}

}  // namespace

SshConfiguredHandler::SshConfiguredHandler() = default;
SshConfiguredHandler::~SshConfiguredHandler() = default;

void SshConfiguredHandler::DeclareJSCallbacks() {
  AddCallback("getIsSshConfigured",
              &SshConfiguredHandler::HandleGetIsSshConfigured);
}

void SshConfiguredHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("sshWarningLogin", IDS_LOGIN_SSH_WARNING);
}

void SshConfiguredHandler::InitAfterJavascriptAllowed() {
  if (callback_ids_.empty())
    return;
  if (!is_ssh_configured_.has_value())
    return;
  ResolveCallbacks();
}

void SshConfiguredHandler::HandleGetIsSshConfigured(
    const std::string& callback_id) {
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
  ResolveCallbacks();
}

void SshConfiguredHandler::ResolveCallbacks() {
  if (!IsJavascriptAllowed())
    return;
  DCHECK(is_ssh_configured_.has_value());
  for (const std::string& callback_id : callback_ids_) {
    ResolveJavascriptCallback(base::Value(callback_id),
                              base::Value(is_ssh_configured_.value()));
  }
  callback_ids_.clear();
}

}  // namespace ash
