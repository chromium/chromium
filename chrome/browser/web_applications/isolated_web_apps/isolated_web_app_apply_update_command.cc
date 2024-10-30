// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"

#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

std::ostream& operator<<(std::ostream& os,
                         const IsolatedWebAppApplyUpdateCommandError& error) {
  return os << "IsolatedWebAppApplyUpdateCommandError { "
               "message = \""
            << error.message << "\" }.";
}

IsolatedWebAppApplyUpdateCommand::IsolatedWebAppApplyUpdateCommand(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(
        base::expected<void, IsolatedWebAppApplyUpdateCommandError>)> callback,
    std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommand<
          AppLock,
          base::expected<void, IsolatedWebAppApplyUpdateCommandError>>(
          "IsolatedWebAppApplyUpdateCommand",
          AppLockDescription(url_info.app_id()),
          std::move(callback), /*args_for_shutdown=*/
          base::unexpected(IsolatedWebAppApplyUpdateCommandError{
              .message = std::string("System is shutting down.")})),
      url_info_(std::move(url_info)),
      web_contents_(std::move(web_contents)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)),
      command_helper_(std::move(command_helper)) {
  CHECK(web_contents_ != nullptr);
  CHECK(optional_profile_keep_alive_ == nullptr ||
        &profile() == optional_profile_keep_alive_->profile());

  GetMutableDebugValue().Set("app_id", url_info_.app_id());
  GetMutableDebugValue().Set("origin", url_info_.origin().Serialize());
  GetMutableDebugValue().Set("bundle_id", url_info_.web_bundle_id().id());
  GetMutableDebugValue().Set(
      "bundle_type", static_cast<int>(url_info_.web_bundle_id().type()));
}

IsolatedWebAppApplyUpdateCommand::~IsolatedWebAppApplyUpdateCommand() = default;

void IsolatedWebAppApplyUpdateCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedWeakCallbacks(
      weak_factory_.GetWeakPtr(),
      &IsolatedWebAppApplyUpdateCommand::CheckIfUpdateIsStillPending,
      &IsolatedWebAppApplyUpdateCommand::CheckTrustAndSignatures,
      &IsolatedWebAppApplyUpdateCommand::HandleKeyRotationIfNecessary,
      &IsolatedWebAppApplyUpdateCommand::CreateStoragePartition,
      &IsolatedWebAppApplyUpdateCommand::PrepareInstallInfo,
      &IsolatedWebAppApplyUpdateCommand::FinalizeUpdate);
}

void IsolatedWebAppApplyUpdateCommand::CheckIfUpdateIsStillPending(
    base::OnceClosure next_step_callback) {
  ASSIGN_OR_RETURN(
      const WebApp& iwa,
      GetIsolatedWebAppById(lock_->registrar(), url_info_.app_id()),
      [&](const std::string& error) { ReportFailure(error); });

  if (!iwa.isolation_data()->pending_update_info().has_value()) {
    ReportFailure("Installed app does not have a pending update.");
    return;
  }

  isolation_data_ = *iwa.isolation_data();

  GetMutableDebugValue().Set("pending_update_info",
                             pending_update_info().AsDebugValue());

  CHECK_GE(pending_update_info().version, isolation_data().version());
  CHECK_EQ(pending_update_info().location.dev_mode(),
           isolation_data().location().dev_mode());

  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  command_helper_->CheckTrustAndSignatures(
      IwaSourceWithMode::FromStorageLocation(profile().GetPath(),
                                             pending_update_info().location),
      &profile(),
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::OnTrustAndSignaturesChecked,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::OnTrustAndSignaturesChecked(
    base::OnceClosure next_step_callback,
    TrustCheckResult trust_check_result) {
  RETURN_IF_ERROR(trust_check_result,
                  [&](const std::string& error) { ReportFailure(error); });
  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::HandleKeyRotationIfNecessary(
    base::OnceClosure next_step_callback) {
  CHECK_GE(pending_update_info().version, isolation_data().version());
  if (pending_update_info().version > isolation_data().version()) {
    // Updates to higher version are always fine.
    std::move(next_step_callback).Run();
    return;
  }

  // Same-version updates are only allowed in case of key rotation, i.e. when
  // the current installation doesn't have the required key but the pending one
  // does.
  CHECK_EQ(pending_update_info().version, isolation_data().version());
  if (LookupRotatedKey(url_info_.web_bundle_id(), GetMutableDebugValue()) ==
      KeyRotationLookupResult::kKeyFound) {
    KeyRotationData data =
        GetKeyRotationData(url_info_.web_bundle_id(), isolation_data());
    if (!data.current_installation_has_rk && data.pending_update_has_rk) {
      std::move(next_step_callback).Run();
      return;
    }
  }

  ReportFailure(base::StringPrintf("Installed app is already on version %s.",
                                   isolation_data().version().GetString()));
}

void IsolatedWebAppApplyUpdateCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::PrepareInstallInfo(
    base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
        next_step_callback) {
  prepare_install_info_job_ = PrepareInstallInfoJob::CreateAndStart(
      profile(),
      IwaSourceWithMode::FromStorageLocation(profile().GetPath(),
                                             pending_update_info().location),
      pending_update_info().version, *web_contents_, *command_helper_,
      lock_->web_contents_manager().CreateUrlLoader(),
      std::move(next_step_callback));
}

void IsolatedWebAppApplyUpdateCommand::FinalizeUpdate(
    PrepareInstallInfoJob::InstallInfoOrFailure result) {
  prepare_install_info_job_.reset();

  ASSIGN_OR_RETURN(
      WebAppInstallInfo install_info, std::move(result),
      [&](const auto& failure) { ReportFailure(failure.message); });

  GetMutableDebugValue().Set("actual_version",
                             install_info.isolated_web_app_version.GetString());
  GetMutableDebugValue().Set("app_title", install_info.title);

  lock_->install_finalizer().FinalizeUpdate(
      install_info,
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::OnFinalized,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppApplyUpdateCommand::OnFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode update_result_code) {
  CHECK_EQ(app_id, url_info_.app_id());

  if (update_result_code ==
      webapps::InstallResultCode::kSuccessAlreadyInstalled) {
    ReportSuccess();
  } else {
    ReportFailure(base::StringPrintf("Error during finalization: %s",
                                     base::ToString(update_result_code)));
  }
}

void IsolatedWebAppApplyUpdateCommand::ReportFailure(std::string_view message) {
  IsolatedWebAppApplyUpdateCommandError error{.message = std::string(message)};
  GetMutableDebugValue().Set("result", "error: " + error.message);

  // If this command fails, then it is best to delete the pending update info
  // from the database. A failed pending update is likely caused by a corrupted
  // Web Bundle. Re-discovering the update and re-downloading the bundle may fix
  // things.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::CleanupOnFailure,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::CompleteAndSelfDestruct,
                     weak_ptr, CommandResult::kFailure,
                     base::unexpected(std::move(error)), FROM_HERE));
}

void IsolatedWebAppApplyUpdateCommand::CleanupOnFailure(
    base::OnceClosure next_step_callback) {
  if (!isolation_data_) {
    std::move(next_step_callback).Run();
    return;
  }

  base::OnceClosure update_callback = base::BindOnce(
      CleanupLocationIfOwned, profile().GetPath(),
      pending_update_info().location, std::move(next_step_callback));

  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(
      // We don't really care whether committing the update succeeds or
      // fails. However, we want to wait for the write of the database to
      // disk, so that a potential crash during that write happens before
      // the to-be-implemented cleanup system for no longer referenced Web
      // Bundles kicks in.
      base::IgnoreArgs<bool>(std::move(update_callback)));

  WebApp& web_app = CHECK_DEREF(update->UpdateApp(url_info_.app_id()));
  web_app.SetIsolationData(IsolationData::Builder(*web_app.isolation_data())
                               .ClearPendingUpdateInfo()
                               .Build());
}

void IsolatedWebAppApplyUpdateCommand::ReportSuccess() {
  GetMutableDebugValue().Set("result", "success");
  CompleteAndSelfDestruct(CommandResult::kSuccess, base::ok());
}

Profile& IsolatedWebAppApplyUpdateCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
