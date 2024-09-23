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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
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
  DETACH_FROM_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lock_ = std::move(lock);
  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::CheckIfUpdateIsStillPending,
          weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::CheckTrustAndSignatures,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::CreateStoragePartition,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::LoadInstallUrl,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::
                         CheckInstallabilityAndRetrieveManifest,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::
                         ValidateManifestAndCreateInstallInfo,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::
                         RetrieveIconsAndPopulateInstallInfo,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::Finalize, weak_ptr));
}

void IsolatedWebAppApplyUpdateCommand::CheckIfUpdateIsStillPending(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ASSIGN_OR_RETURN(
      const WebApp& iwa,
      GetIsolatedWebAppById(lock_->registrar(), url_info_.app_id()),
      [&](const std::string& error) { ReportFailure(error); });
  const IsolationData& isolation_data = *iwa.isolation_data();

  if (!isolation_data.pending_update_info().has_value()) {
    ReportFailure("Installed app does not have a pending update.");
    return;
  }
  pending_update_info_ = *isolation_data.pending_update_info();

  GetMutableDebugValue().Set("pending_update_info",
                             pending_update_info_->AsDebugValue());

  bool same_version_update_allowed_by_key_rotation = false;
  switch (LookupRotatedKey(url_info_.web_bundle_id(), GetMutableDebugValue())) {
    case KeyRotationLookupResult::kNoKeyRotation:
      break;
    case KeyRotationLookupResult::kKeyBlocked:
      ReportFailure(
          "The web bundle id for this app's bundle has been blocked by the key "
          "distribution component.");
      return;
    case KeyRotationLookupResult::kKeyFound: {
      KeyRotationData data =
          GetKeyRotationData(url_info_.web_bundle_id(), isolation_data);
      if (!data.pending_update_has_rk) {
        ReportFailure(
            "The update's integrity block data doesn't contain the required "
            "public key as instructed by the key distribution component -- the "
            "update won't succeed.");
        return;
      }
      if (!data.current_installation_has_rk) {
        same_version_update_allowed_by_key_rotation = true;
      }
    } break;
  }

  if (isolation_data.version() > pending_update_info_->version ||
      (isolation_data.version() == pending_update_info_->version &&
       !same_version_update_allowed_by_key_rotation)) {
    ReportFailure(base::StrCat({"Installed app is already on version ",
                                isolation_data.version().GetString(),
                                ". Cannot update to version ",
                                pending_update_info_->version.GetString()}));
    return;
  }

  if (isolation_data.location().dev_mode() !=
      pending_update_info_->location.dev_mode()) {
    std::stringstream s;
    s << "Unable to update between dev-mode and non-dev-mode storage location "
         "types ("
      << isolation_data.location() << " to " << pending_update_info_->location
      << ").";
    ReportFailure(s.str());
    return;
  }

  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  command_helper_->CheckTrustAndSignatures(
      IwaSourceWithMode::FromStorageLocation(profile().GetPath(),
                                             pending_update_info_->location),
      &profile(),
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<void>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(cmfcmf): Maybe we should log somewhere when the storage partition is
  // unexpectedly missing?
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_helper_->LoadInstallUrl(
      IwaSourceWithMode::FromStorageLocation(profile().GetPath(),
                                             pending_update_info_->location),
      *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<void>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<
                         blink::mojom::ManifestPtr>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::ValidateManifestAndCreateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    blink::mojom::ManifestPtr manifest) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(
          pending_update_info_->version, *manifest);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void IsolatedWebAppApplyUpdateCommand::RetrieveIconsAndPopulateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    WebAppInstallInfo install_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetMutableDebugValue().Set("app_title", install_info.title);

  command_helper_->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), *web_contents_.get(),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<
                         WebAppInstallInfo>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::Finalize(WebAppInstallInfo info) {
  lock_->install_finalizer().FinalizeUpdate(
      info, base::BindOnce(&IsolatedWebAppApplyUpdateCommand::OnFinalized,
                           weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppApplyUpdateCommand::OnFinalized(
    const webapps::AppId& app_id,
    webapps::InstallResultCode update_result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(app_id, url_info_.app_id());

  if (update_result_code ==
      webapps::InstallResultCode::kSuccessAlreadyInstalled) {
    ReportSuccess();
  } else {
    std::stringstream ss;
    ss << "Error during finalization: " << update_result_code;
    ReportFailure(ss.str());
  }
}

void IsolatedWebAppApplyUpdateCommand::ReportFailure(std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  base::OnceClosure update_callback =
      pending_update_info_.has_value()
          ? base::BindOnce(CleanupLocationIfOwned, profile().GetPath(),
                           pending_update_info_->location,
                           std::move(next_step_callback))
          : std::move(next_step_callback);

  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(
      // We don't really care whether committing the update succeeds or
      // fails. However, we want to wait for the write of the database to
      // disk, so that a potential crash during that write happens before
      // the to-be-implemented cleanup system for no longer referenced Web
      // Bundles kicks in.
      base::IgnoreArgs<bool>(std::move(update_callback)));

  WebApp* web_app = update->UpdateApp(url_info_.app_id());

  // This command might fail because the app is no longer installed, or
  // because it does not have `IsolationData` or
  // `IsolationData::PendingUpdateInfo`, in which case there is no
  // pending update info for us to delete.
  if (!web_app || !web_app->isolation_data().has_value() ||
      !web_app->isolation_data()->pending_update_info().has_value()) {
    return;
  }

  web_app->SetIsolationData(IsolationData::Builder(*web_app->isolation_data())
                                .ClearPendingUpdateInfo()
                                .Build());
}

void IsolatedWebAppApplyUpdateCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetMutableDebugValue().Set("result", "success");
  CompleteAndSelfDestruct(CommandResult::kSuccess, base::ok());
}

Profile& IsolatedWebAppApplyUpdateCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
