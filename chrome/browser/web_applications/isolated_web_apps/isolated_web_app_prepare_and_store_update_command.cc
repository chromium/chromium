// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include <array>
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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_version.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace web_app {

IsolatedWebAppUpdatePrepareAndStoreCommand::
    IsolatedWebAppUpdatePrepareAndStoreCommand(
        WebApp::IsolationData::PendingUpdateInfo update_info,
        IsolatedWebAppUrlInfo url_info,
        std::unique_ptr<content::WebContents> web_contents,
        std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
        std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
        base::OnceCallback<void(
            base::expected<void,
                           IsolatedWebAppUpdatePrepareAndStoreCommandError>)>
            callback,
        std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommandTemplate<AppLock>(
          "IsolatedWebAppUpdatePrepareAndStoreCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(url_info.app_id())),
      update_info_(std::move(update_info)),
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
                        static_cast<int>(url_info_.web_bundle_id().type()))
                   .Set("update_info", update_info_.AsDebugValue());
}

IsolatedWebAppUpdatePrepareAndStoreCommand::
    ~IsolatedWebAppUpdatePrepareAndStoreCommand() = default;

const LockDescription&
IsolatedWebAppUpdatePrepareAndStoreCommand::lock_description() const {
  return *lock_description_;
}

base::Value IsolatedWebAppUpdatePrepareAndStoreCommand::ToDebugValue() const {
  return base::Value(debug_log_.Clone());
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lock_ = std::move(lock);
  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::
                         CheckIfUpdateIsStillApplicable,
                     weak_ptr),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::CheckTrustAndSignatures,
          weak_ptr),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::CreateStoragePartition,
          weak_ptr),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::LoadInstallUrl,
          weak_ptr),
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::
                         CheckInstallabilityAndRetrieveManifest,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::
                         ValidateManifestAndCreateInstallInfo,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::
                         RetrieveIconsAndPopulateInstallInfo,
                     weak_ptr),
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::Finalize,
                     weak_ptr));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CheckIfUpdateIsStillApplicable(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const WebApp* installed_app =
      lock_->registrar().GetAppById(url_info_.app_id());
  if (installed_app == nullptr) {
    ReportFailure("App is no longer installed.");
    return;
  }
  if (!installed_app->isolation_data().has_value()) {
    ReportFailure("Installed app is not an Isolated Web App.");
    return;
  }
  if (installed_app->isolation_data()->version >= update_info_.version) {
    ReportFailure(base::StrCat(
        {"Installed app is already on version ",
         installed_app->isolation_data()->version.GetString(),
         ". Cannot update to version ", update_info_.version.GetString()}));
    return;
  }
  if (installed_app->isolation_data()->location.index() !=
      update_info_.location.index()) {
    ReportFailure(
        base::StringPrintf("Unable to update between different "
                           "IsolatedWebAppLocation types (%zu to %zu).",
                           installed_app->isolation_data()->location.index(),
                           update_info_.location.index()));
    return;
  }

  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  command_helper_->CheckTrustAndSignatures(
      update_info_.location, &profile(),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::RunNextStepOnSuccess<
              void>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(cmfcmf): Maybe we should log somewhere when the storage partition is
  // unexpectedly missing?
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_helper_->LoadInstallUrl(
      update_info_.location, *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::RunNextStepOnSuccess<
              void>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::
    CheckInstallabilityAndRetrieveManifest(
        base::OnceCallback<
            void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
            next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::RunNextStepOnSuccess<
              IsolatedWebAppInstallCommandHelper::ManifestAndUrl>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::
    ValidateManifestAndCreateInstallInfo(
        base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
        IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(
          update_info_.version, manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::
    RetrieveIconsAndPopulateInstallInfo(
        base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
        WebAppInstallInfo install_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  debug_log_.Set("app_title", install_info.title);

  command_helper_->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), *web_contents_.get(),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::RunNextStepOnSuccess<
              WebAppInstallInfo>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::Finalize(
    WebAppInstallInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::OnFinalized,
                     weak_factory_.GetWeakPtr()));

  WebApp* app_to_update = update->UpdateApp(url_info_.app_id());
  CHECK(app_to_update);

  WebApp::IsolationData updated_isolation_data =
      *app_to_update->isolation_data();
  updated_isolation_data.SetPendingUpdateInfo(update_info_);
  app_to_update->SetIsolationData(std::move(updated_isolation_data));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::OnFinalized(bool success) {
  if (success) {
    ReportSuccess();
  } else {
    ReportFailure("Failed to save pending update info to Web App Database.");
  }
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::OnShutdown() {
  // TODO(cmfcmf): Test cancellation of pending update during system
  // shutdown.
  ReportFailure("System is shutting down.");
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportFailure(
    base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  IsolatedWebAppUpdatePrepareAndStoreCommandError error{
      .message = std::string(message)};
  debug_log_.Set("result", "error: " + error.message);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), base::unexpected(std::move(error))));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  debug_log_.Set("result", "success");
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_), base::ok()));
}

Profile& IsolatedWebAppUpdatePrepareAndStoreCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
