// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/enterprise_companion_client.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/mojom/enterprise_companion.mojom.h"
#include "components/named_mojo_ipc_server/named_mojo_ipc_server_client_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace enterprise_companion {

const char kEnableUsageStatsSwitch[] = "enable-usage-stats";
const char kInstallIfNeededSwitch[] = "install-if-needed";

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kServerName[] = MAC_BUNDLE_IDENTIFIER_STRING ".service";
#elif BUILDFLAG(IS_LINUX)
constexpr char kServerName[] =
    "/run/" COMPANY_SHORTNAME_STRING "/" PRODUCT_FULLNAME_STRING "/service.sk";
#elif BUILDFLAG(IS_WIN)
constexpr wchar_t kServerName[] = PRODUCT_FULLNAME_STRING L"Service";
#endif

bool LaunchEnterpriseCompanionApp(bool enable_usagestats) {
  std::optional<base::FilePath> binary_path = FindExistingInstall();
  if (!binary_path) {
    return false;
  }

  base::CommandLine command_line = base::CommandLine(*binary_path);
  if (enable_usagestats) {
    command_line.AppendSwitch(kEnableUsageStatsSwitch);
  }
  return base::LaunchProcess(command_line, {}).IsValid();
}

void OnEndpointReceived(
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>,
                            mojo::Remote<mojom::EnterpriseCompanion>)> callback,
    mojo::PlatformChannelEndpoint endpoint) {
  if (!endpoint.is_valid()) {
    std::move(callback).Run(nullptr, {});
    return;
  }

  std::unique_ptr<mojo::IsolatedConnection> connection =
      std::make_unique<mojo::IsolatedConnection>();
  mojo::Remote<mojom::EnterpriseCompanion> remote(
      mojo::PendingRemote<mojom::EnterpriseCompanion>(
          connection->Connect(std::move(endpoint)),
          /*version=*/0));
  std::move(callback).Run(std::move(connection), std::move(remote));
}

// Repeatedly attempts to connect to the remote service until `deadline` is
// exhausted. If the service could not be reached after the first attempt, the
// application is launched.
void ConnectWithRetries(
    const mojo::NamedPlatformChannel::ServerName& server_name,
    const base::Clock* clock,
    int tries,
    base::Time deadline,
    bool enable_usagestats,
    base::OnceCallback<void(mojo::PlatformChannelEndpoint)> callback) {
  if (clock->Now() > deadline) {
    VLOG(1) << "Failed to connect to EnterpriseCompanionService remote. "
               "Connection timed out.";
    std::move(callback).Run({});
    return;
  }

  if (tries == 1 && !LaunchEnterpriseCompanionApp(enable_usagestats)) {
    VLOG(1) << "Failed to connect to EnterpriseCompanionService remote. "
               "The service could not be launched.";
    std::move(callback).Run({});
    return;
  }

  mojo::PlatformChannelEndpoint endpoint =
      named_mojo_ipc_server::ConnectToServer(server_name);
  if (endpoint.is_valid()) {
    std::move(callback).Run(std::move(endpoint));
    return;
  }

  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectWithRetries, server_name, clock, tries + 1,
                     deadline, enable_usagestats, std::move(callback)),
      base::Milliseconds(30 * tries));
}

}  // namespace

mojo::NamedPlatformChannel::ServerName GetServerName() {
  return kServerName;
}

void ConnectToServer(
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>,
                            mojo::Remote<mojom::EnterpriseCompanion>)> callback,
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const mojo::NamedPlatformChannel::ServerName& server_name) {
            return named_mojo_ipc_server::ConnectToServer(server_name);
          },
          server_name)
          .Then(base::BindPostTaskToCurrentDefault(
              base::BindOnce(&OnEndpointReceived, std::move(callback)))));
}

void ConnectAndLaunchServer(
    const base::Clock* clock,
    base::TimeDelta timeout,
    bool enable_usagestats,
    base::OnceCallback<void(std::unique_ptr<mojo::IsolatedConnection>,
                            mojo::Remote<mojom::EnterpriseCompanion>)> callback,
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ConnectWithRetries, server_name, clock, /*tries=*/0,
                     clock->Now() + timeout, enable_usagestats,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &OnEndpointReceived, std::move(callback)))));
}

}  // namespace enterprise_companion
