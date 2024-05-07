// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/remove_uninstalled_apps_task.h"

#include <optional>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/update_service_impl.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"

namespace updater {

namespace {

struct AppInfo {
  AppInfo(const std::string& app_id,
          const base::Version& app_version,
          const base::FilePath& ecp)
      : app_id_(app_id), app_version_(app_version), ecp_(ecp) {}
  std::string app_id_;
  base::Version app_version_;
  base::FilePath ecp_;
};

struct PingInfo {
  PingInfo(const std::string& app_id,
           const base::Version& app_version,
           int ping_reason)
      : app_id_(app_id), app_version_(app_version), ping_reason_(ping_reason) {}
  std::string app_id_;
  base::Version app_version_;
  int ping_reason_;
};

std::vector<AppInfo> GetRegisteredApps(
    scoped_refptr<updater::PersistedData> persisted_data) {
  std::vector<AppInfo> apps;
  for (const std::string& app_id : persisted_data->GetAppIds()) {
    if (!base::EqualsCaseInsensitiveASCII(app_id, kUpdaterAppId)) {
      apps.emplace_back(app_id, persisted_data->GetProductVersion(app_id),
                        persisted_data->GetExistenceCheckerPath(app_id));
    }
  }
  return apps;
}

std::vector<PingInfo> GetAppIDsToRemove(
    const std::vector<AppInfo>& apps,
    base::RepeatingCallback<std::optional<int>(const std::string&,
                                               const base::FilePath&)>
        predicate) {
  std::vector<PingInfo> app_ids_to_remove;
  for (const auto& app : apps) {
    std::optional<int> remove_reason = predicate.Run(app.app_id_, app.ecp_);
    if (remove_reason) {
      app_ids_to_remove.emplace_back(app.app_id_, app.app_version_,
                                     *remove_reason);
    }
  }
  return app_ids_to_remove;
}

void UninstallPingSent(base::RepeatingClosure callback,
                       update_client::Error error) {
  if (error != update_client::Error::NONE) {
    VLOG(0) << __func__ << ": Error: " << error;
  }
  callback.Run();
}

void RemoveAppIDsAndSendUninstallPings(
    base::OnceClosure callback,
    scoped_refptr<PersistedData> persisted_data,
    scoped_refptr<update_client::UpdateClient> update_client,
    const std::vector<PingInfo>& app_ids_to_remove) {
  if (app_ids_to_remove.empty()) {
    std::move(callback).Run();
    return;
  }

  // If the terms of service have not been accepted, don't ping.
  if (persisted_data->GetEulaRequired()) {
    for (const PingInfo& app_id_to_remove : app_ids_to_remove) {
      const std::string& app_id = app_id_to_remove.app_id_;
      if (!persisted_data->RemoveApp(app_id)) {
        VLOG(0) << "Could not remove registration of app " << app_id;
      }
    }
    std::move(callback).Run();
    return;
  }

  const auto barrier_closure =
      base::BarrierClosure(app_ids_to_remove.size(), std::move(callback));

  for (const PingInfo& app_id_to_remove : app_ids_to_remove) {
    const std::string& app_id = app_id_to_remove.app_id_;
    const int ping_reason = app_id_to_remove.ping_reason_;
    const base::Version& app_version = app_id_to_remove.app_version_;
    const std::string& brand = persisted_data->GetBrandCode(app_id);
    const std::string& ap = persisted_data->GetAP(app_id);

    if (persisted_data->RemoveApp(app_id)) {
      VLOG(1) << "Uninstall ping for app id: " << app_id
              << ". Ping reason: " << ping_reason;
      update_client::CrxComponent crx_component;
      crx_component.ap = ap;
      crx_component.app_id = app_id;
      crx_component.brand = brand;
      crx_component.version = app_version;
      crx_component.requires_network_encryption = false;
      update_client->SendPing(
          crx_component,
          {.event_type = update_client::protocol_request::kEventUninstall,
           .result = 1,
           .error_code = 0,
           .extra_code1 = ping_reason},
          base::BindOnce(&UninstallPingSent, barrier_closure));
    } else {
      VLOG(0) << "Could not remove registration of app " << app_id;
    }
  }
}

}  // namespace

RemoveUninstalledAppsTask::RemoveUninstalledAppsTask(
    scoped_refptr<Configurator> config,
    UpdaterScope scope)
    : config_(config),
      update_client_(update_client::UpdateClientFactory(config_)),
      scope_(scope) {}

RemoveUninstalledAppsTask::~RemoveUninstalledAppsTask() = default;

void RemoveUninstalledAppsTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          GetAppIDsToRemove,
          GetRegisteredApps(config_->GetUpdaterPersistedData()),
          base::BindRepeating(&RemoveUninstalledAppsTask::GetUnregisterReason,
                              this)),
      base::BindOnce(&RemoveAppIDsAndSendUninstallPings, std::move(callback),
                     config_->GetUpdaterPersistedData(), update_client_));
}

}  // namespace updater
