// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

IsolatedWebAppApplyUpdateCommand::IsolatedWebAppApplyUpdateCommand(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(
        base::expected<void, IsolatedWebAppApplyUpdateCommandError>)> callback,
    std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommandTemplate<AppLock>("IsolatedWebAppApplyUpdateCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(url_info.app_id())),
      url_info_(std::move(url_info)),
      web_contents_(std::move(web_contents)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)),
      callback_(std::move(callback)),
      command_helper_(std::move(command_helper)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  CHECK(web_contents_ != nullptr);
  CHECK(optional_profile_keep_alive_ == nullptr ||
        &profile() == optional_profile_keep_alive_->profile());

  debug_log_ = base::Value::Dict()
                   .Set("app_id", url_info_.app_id())
                   .Set("origin", url_info_.origin().Serialize())
                   .Set("bundle_id", url_info_.web_bundle_id().id())
                   .Set("bundle_type",
                        static_cast<int>(url_info_.web_bundle_id().type()));
}

IsolatedWebAppApplyUpdateCommand::~IsolatedWebAppApplyUpdateCommand() = default;

const LockDescription& IsolatedWebAppApplyUpdateCommand::lock_description()
    const {
  return *lock_description_;
}

base::Value IsolatedWebAppApplyUpdateCommand::ToDebugValue() const {
  return base::Value(debug_log_.Clone());
}

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

  installed_app_ = lock_->registrar().GetAppById(url_info_.app_id());
  if (installed_app_ == nullptr) {
    ReportFailure("App is no longer installed.");
    return;
  }
  if (!installed_app_->isolation_data().has_value()) {
    ReportFailure("Installed app is not an Isolated Web App.");
    return;
  }
  const WebApp::IsolationData& isolation_data =
      *installed_app_->isolation_data();

  if (!isolation_data.pending_update_info().has_value()) {
    ReportFailure("Installed app does not have a pending update.");
    return;
  }
  const WebApp::IsolationData::PendingUpdateInfo& update_info =
      *isolation_data.pending_update_info();

  debug_log_.Set("update_info", update_info.AsDebugValue());

  if (isolation_data.version >= update_info.version) {
    ReportFailure(base::StrCat({"Installed app is already on version ",
                                isolation_data.version.GetString(),
                                ". Cannot update to version ",
                                update_info.version.GetString()}));
    return;
  }
  if (isolation_data.location.index() != update_info.location.index()) {
    ReportFailure(base::StringPrintf(
        "Unable to update between different "
        "IsolatedWebAppLocation types (%zu to %zu).",
        isolation_data.location.index(), update_info.location.index()));
    return;
  }

  std::move(next_step_callback).Run();
}

void IsolatedWebAppApplyUpdateCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  command_helper_->CheckTrustAndSignatures(
      update_info().location, &profile(),
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
      update_info().location, *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<void>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
        next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(&IsolatedWebAppApplyUpdateCommand::RunNextStepOnSuccess<
                         IsolatedWebAppInstallCommandHelper::ManifestAndUrl>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void IsolatedWebAppApplyUpdateCommand::ValidateManifestAndCreateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(
          update_info().version, manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void IsolatedWebAppApplyUpdateCommand::RetrieveIconsAndPopulateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    WebAppInstallInfo install_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  debug_log_.Set("app_title", install_info.title);

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
    webapps::InstallResultCode update_result_code,
    OsHooksErrors unused_os_hooks_errors) {
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

void IsolatedWebAppApplyUpdateCommand::OnShutdown() {
  // Stop any potential ongoing operations by destroying the `command_helper_`.
  command_helper_.reset();

  // TODO(cmfcmf): Test cancellation of pending update during system
  // shutdown.
  ReportFailure("System is shutting down.", /*due_to_shutdown=*/true);
}

void IsolatedWebAppApplyUpdateCommand::ReportFailure(base::StringPiece message,
                                                     bool due_to_shutdown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  IsolatedWebAppApplyUpdateCommandError error{.message = std::string(message)};
  debug_log_.Set("result", "error: " + error.message);

  if (due_to_shutdown) {
    SignalCompletionAndSelfDestruct(
        CommandResult::kShutdown,
        base::BindOnce(std::move(callback_),
                       base::unexpected(std::move(error))));
    return;
  }

  // If this command fails, then it is best to delete the pending update info
  // from the database. A failed pending update is likely caused by a corrupted
  // Web Bundle. Re-discovering the update and re-downloading the bundle may fix
  // things.
  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::CleanupUpdateInfoOnFailure,
          weak_ptr),
      base::BindOnce(
          &IsolatedWebAppApplyUpdateCommand::SignalCompletionAndSelfDestruct,
          weak_ptr, CommandResult::kFailure,
          base::BindOnce(std::move(callback_),
                         base::unexpected(std::move(error))))

  );
}

void IsolatedWebAppApplyUpdateCommand::CleanupUpdateInfoOnFailure(
    base::OnceClosure next_step_callback) {
  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(
      // We don't really care whether committing the update succeeds or fails.
      // However, we want to wait for the write of the database to disk, so that
      // a potential crash during that write happens before the
      // to-be-implemented cleanup system for no longer referenced Web Bundles
      // kicks in.
      base::IgnoreArgs<bool>(std::move(next_step_callback)));

  WebApp* web_app = update->UpdateApp(url_info_.app_id());

  // This command might fail because the app is no longer installed, or because
  // it does not have `WebApp::IsolationData` or
  // `WebApp::IsolationData::PendingUpdateInfo`, in which case there is no
  // pending update info for us to delete.
  if (!web_app || !web_app->isolation_data().has_value() ||
      !web_app->isolation_data()->pending_update_info().has_value()) {
    return;
  }

  WebApp::IsolationData updated_isolation_data = *web_app->isolation_data();
  updated_isolation_data.SetPendingUpdateInfo(absl::nullopt);
  web_app->SetIsolationData(std::move(updated_isolation_data));
}

void IsolatedWebAppApplyUpdateCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  debug_log_.Set("result", "success");
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_), base::ok()));
}

Profile& IsolatedWebAppApplyUpdateCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
