// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    const base::FilePath profile_path,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback) {
  // Protocol handlers are managed through app shims. However when creating
  // those shims, we do need to know protocol handlers for all profiles an app
  // is installed in. As such, persist the protocol handler information in
  // AppShimRegistry.
  std::set<std::string> protocols;
  for (const auto& handler : protocol_handlers) {
    if (!handler.protocol.empty())
      protocols.insert(handler.protocol);
  }
  AppShimRegistry::Get()->SaveProtocolHandlersForAppAndProfile(
      app_id, profile_path, std::move(protocols));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

void UnregisterProtocolHandlersWithOs(const webapps::AppId& app_id,
                                      const base::FilePath profile_path,
                                      ResultCallback callback) {
  AppShimRegistry::Get()->SaveProtocolHandlersForAppAndProfile(
      app_id, profile_path, {});
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), Result::kOk));
}

}  // namespace web_app
