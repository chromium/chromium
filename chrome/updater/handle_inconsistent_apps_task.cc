// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/handle_inconsistent_apps_task.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/installer.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/ping_configurator.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace updater {
namespace {

struct ProductInfo {
  std::string app_id;
  base::Version pv;
  std::string pv_key;
  base::FilePath pv_path;
};

std::vector<update_client::CrxComponent> MakeOverinstallPings(
    UpdaterScope scope,
    std::vector<ProductInfo> products) {
  std::vector<update_client::CrxComponent> pings;
  for (const ProductInfo& product : products) {
    const base::Version actual_version =
        LookupVersion(scope, product.app_id, product.pv_path, product.pv_key,
                      base::Version());
    if (!actual_version.IsValid()) {
      VLOG(2) << "Failed to lookup version for " << product.app_id
              << ". Not sending an overinstall ping.";
      continue;
    } else if (product.pv == actual_version) {
      continue;
    }

    VLOG(1) << "App " << product.app_id
            << " has a different version than what is installed. Expected: "
            << product.pv << ", actually installed: " << actual_version
            << ". An install ping will be sent.";

    update_client::CrxComponent ping_data;
    ping_data.app_id = product.app_id;
    ping_data.version = actual_version;
    ping_data.requires_network_encryption = false;
    pings.push_back(ping_data);
  }
  return pings;
}

}  // namespace

HandleInconsistentAppsTask::HandleInconsistentAppsTask(
    scoped_refptr<Configurator> config,
    UpdaterScope scope)
    : config_(config),
      scope_(scope),
      blocking_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {}

HandleInconsistentAppsTask::~HandleInconsistentAppsTask() = default;

void HandleInconsistentAppsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FindUnregisteredApps(
      base::BindOnce(&HandleInconsistentAppsTask::PingOverinstalledApps,
                     base::WrapRefCounted(this), std::move(callback)));
}

void HandleInconsistentAppsTask::FindUnregisteredApps(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MigrateLegacyUpdaters(
      scope_, base::BindRepeating(
                  [](scoped_refptr<PersistedData> persisted_data,
                     const RegistrationRequest& req) {
                    if (!base::Contains(persisted_data->GetAppIds(),
                                        base::ToLowerASCII(req.app_id))) {
                      VLOG(1) << "Registering app from legacy updater: "
                              << req.app_id;
                      persisted_data->RegisterApp(req);
                    }
                  },
                  config_->GetUpdaterPersistedData()));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void HandleInconsistentAppsTask::PingOverinstalledApps(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<PersistedData> prefs = config_->GetUpdaterPersistedData();
  std::vector<ProductInfo> products;
  for (const std::string& app_id : prefs->GetAppIds()) {
    products.push_back({.app_id = app_id,
                        .pv = prefs->GetProductVersion(app_id),
                        .pv_key = prefs->GetProductVersionKey(app_id),
                        .pv_path = prefs->GetProductVersionPath(app_id)});
  }
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MakeOverinstallPings, scope_, std::move(products)),
      base::BindOnce(&HandleInconsistentAppsTask::SendOverinstallPings,
                     base::WrapRefCounted(this), std::move(callback)));
}

void HandleInconsistentAppsTask::SendOverinstallPings(
    base::OnceClosure callback,
    std::vector<update_client::CrxComponent> pings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pings.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  const update_client::CrxComponent ping_data = pings.back();
  pings.pop_back();
  config_->GetUpdaterPersistedData()->SetProductVersion(ping_data.app_id,
                                                        ping_data.version);

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const update_client::CrxComponent& ping_data,
             base::OnceClosure callback) {
            update_client::UpdateClientFactory(CreatePingConfigurator())
                ->SendPing(
                    ping_data,
                    {.event_type =
                         update_client::protocol_request::kEventInstall,
                     .result =
                         update_client::protocol_request::kEventResultSuccess},
                    base::BindOnce([](update_client::Error error) {
                      VLOG_IF(1, error != update_client::Error::NONE)
                          << "Failed to send overinstall ping: " << error;
                    }).Then(std::move(callback)));
          },
          ping_data,
          base::BindPostTaskToCurrentDefault(
              base::BindOnce(&HandleInconsistentAppsTask::SendOverinstallPings,
                             base::WrapRefCounted(this), std::move(callback),
                             std::move(pings)))));
}

}  // namespace updater
