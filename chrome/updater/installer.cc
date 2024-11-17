// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/installer.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/action_handler.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_usage_stats_task.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace updater {

namespace {

// Runs in thread pool, can block.
AppInfo MakeAppInfo(UpdaterScope scope,
                    const std::string& app_id,
                    const base::Version& pv,
                    const base::FilePath& pv_path,
                    const std::string& pv_key,
                    const std::string& ap,
                    const base::FilePath& ap_path,
                    const std::string& ap_key,
                    const std::string& brand,
                    const base::FilePath& brand_path,
                    const std::string& brand_key,
                    const base::FilePath& ec_path) {
  const base::Version pv_lookup =
      LookupVersion(scope, app_id, pv_path, pv_key, pv);
  return AppInfo(scope, app_id, LookupString(ap_path, ap_key, ap),
                 LookupString(brand_path, brand_key, brand),
                 pv_lookup.IsValid() ? pv_lookup : base::Version(kNullVersion),
                 ec_path);
}

}  // namespace

AppInfo::AppInfo(const UpdaterScope scope,
                 const std::string& app_id,
                 const std::string& ap,
                 const std::string& brand,
                 const base::Version& app_version,
                 const base::FilePath& ecp)
    : scope(scope),
      app_id(app_id),
      ap(ap),
      brand(brand),
      version(app_version),
      ecp(ecp) {}
AppInfo::AppInfo(const AppInfo&) = default;
AppInfo& AppInfo::operator=(const AppInfo&) = default;
AppInfo::~AppInfo() = default;

Installer::Installer(
    const std::string& app_id,
    const std::string& client_install_data,
    const std::string& install_data_index,
    const std::string& install_source,
    const std::string& target_channel,
    const std::string& target_version_prefix,
    bool rollback_allowed,
    bool update_disabled,
    UpdateService::PolicySameVersionUpdate policy_same_version_update,
    scoped_refptr<PersistedData> persisted_data,
    crx_file::VerifierFormat crx_verifier_format)
    : updater_scope_(GetUpdaterScope()),
      app_id_(app_id),
      client_install_data_(client_install_data),
      install_data_index_(install_data_index),
      install_source_(install_source),
      rollback_allowed_(rollback_allowed),
      target_channel_(target_channel),
      target_version_prefix_(target_version_prefix),
      update_disabled_(update_disabled),
      policy_same_version_update_(policy_same_version_update),
      persisted_data_(persisted_data),
      crx_verifier_format_(crx_verifier_format),
      usage_stats_enabled_(persisted_data->GetUsageStatsEnabled() ||
                           AreRawUsageStatsEnabled(updater_scope_)),
      app_info_(AppInfo(GetUpdaterScope(), app_id, {}, {}, {}, {})) {}

Installer::~Installer() = default;

void Installer::MakeCrxComponent(
    base::OnceCallback<void(update_client::CrxComponent)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&MakeAppInfo, updater_scope_, app_id_,
                     persisted_data_->GetProductVersion(app_id_),
                     persisted_data_->GetProductVersionPath(app_id_),
                     persisted_data_->GetProductVersionKey(app_id_),
                     persisted_data_->GetAP(app_id_),
                     persisted_data_->GetAPPath(app_id_),
                     persisted_data_->GetAPKey(app_id_),
                     persisted_data_->GetBrandCode(app_id_),
                     persisted_data_->GetBrandPath(app_id_), "KSBrandID",
                     persisted_data_->GetExistenceCheckerPath(app_id_)),
      base::BindOnce(&Installer::MakeCrxComponentFromAppInfo, this,
                     std::move(callback)));
}

void Installer::MakeCrxComponentFromAppInfo(
    base::OnceCallback<void(update_client::CrxComponent)> callback,
    const AppInfo& app_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << __func__ << " for " << app_id_;

  app_info_ = app_info;

  update_client::CrxComponent component;
  component.installer = scoped_refptr<Installer>(this);
  component.action_handler = MakeActionHandler();
  component.requires_network_encryption = false;
  component.crx_format_requirement = crx_verifier_format_;
  component.app_id = app_id_;

  // Query server for install data only when the client does not specify one.
  if (client_install_data_.empty()) {
    component.install_data_index = install_data_index_;
  }

  component.ap = app_info_.ap;
  component.brand = app_info_.brand;
  component.name = app_id_;
  component.version = app_info_.version;
  component.fingerprint = persisted_data_->GetFingerprint(app_id_);
  component.channel = target_channel_;
  component.rollback_allowed = rollback_allowed_;
  component.same_version_update_allowed =
      policy_same_version_update_ ==
      UpdateService::PolicySameVersionUpdate::kAllowed;
  component.target_version_prefix = target_version_prefix_;
  component.updates_enabled = !update_disabled_;
  component.install_source = install_source_;

  std::move(callback).Run(component);
}

Installer::Result Installer::InstallHelper(
    const base::FilePath& unpack_path,
    std::unique_ptr<InstallParams> install_params,
    ProgressCallback progress_callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  VLOG(1) << "Installing update for " << app_id_;

  // Resolve the path to an installer file, which is included in the CRX, and
  // specified by the |run| attribute in the manifest object of an update
  // response.
  if (!install_params || install_params->run.empty()) {
    return Result(GOOPDATEINSTALL_E_FILENAME_INVALID,
                  kErrorMissingInstallParams);
  }

  // Assume the install params are ASCII for now.
  // Upon success, when the control flow returns back to the |update_client|,
  // the prefs are updated asynchronously with the new |pv| and |fingerprint|.
  // The task sequencing guarantees that the prefs will be updated by the
  // time another CrxDataCallback is invoked, which needs updated values.
  return RunApplicationInstaller(
      app_info_, unpack_path.AppendASCII(install_params->run),
      install_params->arguments,
      WriteInstallerDataToTempFile(unpack_path,
                                   client_install_data_.empty()
                                       ? install_params->server_install_data
                                       : client_install_data_),
      usage_stats_enabled_, kWaitForAppInstaller, std::move(progress_callback));
}

void Installer::InstallWithSyncPrimitives(
    const base::FilePath& unpack_path,
    std::unique_ptr<InstallParams> install_params,
    ProgressCallback progress_callback,
    Callback callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  const auto result = InstallHelper(unpack_path, std::move(install_params),
                                    std::move(progress_callback));
  std::move(callback).Run(result);
}

void Installer::OnUpdateError(int error) {
  LOG(ERROR) << "updater error: " << error << " for " << app_id_;
}

void Installer::Install(const base::FilePath& unpack_path,
                        const std::string& /*public_key*/,
                        std::unique_ptr<InstallParams> install_params,
                        ProgressCallback progress_callback,
                        Callback callback) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&Installer::InstallWithSyncPrimitives, this, unpack_path,
                     std::move(install_params), std::move(progress_callback),
                     std::move(callback)));
}

std::optional<base::FilePath> Installer::GetInstalledFile(
    const std::string& file) {
  return std::nullopt;
}

bool Installer::Uninstall() {
  return false;
}

}  // namespace updater
