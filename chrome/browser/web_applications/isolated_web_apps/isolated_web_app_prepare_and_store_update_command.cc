// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include <memory>
#include <optional>
#include <ostream>
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
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::
    IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
        base::Version update_version,
        IsolatedWebAppStorageLocation destination_location)
    : update_version(std::move(update_version)),
      location(std::move(destination_location)) {}
IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::
    ~IsolatedWebAppUpdatePrepareAndStoreCommandSuccess() = default;
IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::
    IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
        const IsolatedWebAppUpdatePrepareAndStoreCommandSuccess& other) =
        default;

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdatePrepareAndStoreCommandSuccess& success) {
  return os << "IsolatedWebAppUpdatePrepareAndStoreCommandSuccess { "
               "update_version = \""
            << success.update_version.GetString() << "\" }.";
}

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdatePrepareAndStoreCommandError& error) {
  return os << "IsolatedWebAppUpdatePrepareAndStoreCommandError { "
               "message = \""
            << error.message << "\" }.";
}

IsolatedWebAppUpdatePrepareAndStoreCommand::
    IsolatedWebAppUpdatePrepareAndStoreCommand(
        UpdateInfo update_info,
        IsolatedWebAppUrlInfo url_info,
        std::unique_ptr<content::WebContents> web_contents,
        std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
        std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
        base::OnceCallback<
            void(IsolatedWebAppUpdatePrepareAndStoreCommandResult)> callback,
        std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommand<AppLock, IsolatedWebAppUpdatePrepareAndStoreCommandResult>(
          "IsolatedWebAppUpdatePrepareAndStoreCommand",
          AppLockDescription(url_info.app_id()),
          std::move(callback), /*args_for_shutdown=*/
          base::unexpected(IsolatedWebAppUpdatePrepareAndStoreCommandError{
              .message = "System is shutting down."})),
      command_helper_(std::move(command_helper)),
      url_info_(std::move(url_info)),
      expected_version_(update_info.expected_version()),
      update_source_(update_info.source()),
      web_contents_(std::move(web_contents)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  CHECK(web_contents_ != nullptr);
  CHECK(optional_profile_keep_alive_ == nullptr ||
        &profile() == optional_profile_keep_alive_->profile());

  GetMutableDebugValue().Set("app_id", url_info_.app_id());
  GetMutableDebugValue().Set("origin", url_info_.origin().Serialize());
  GetMutableDebugValue().Set("bundle_id", url_info_.web_bundle_id().id());
  GetMutableDebugValue().Set(
      "bundle_type", static_cast<int>(url_info_.web_bundle_id().type()));
  GetMutableDebugValue().Set("update_source", update_source_->ToDebugValue());
  GetMutableDebugValue().Set("expected_version",
                             expected_version_.has_value()
                                 ? expected_version_->GetString()
                                 : "unknown");
}

IsolatedWebAppUpdatePrepareAndStoreCommand::
    ~IsolatedWebAppUpdatePrepareAndStoreCommand() {
  if (destination_storage_location_.has_value()) {
    CleanupLocationIfOwned(profile().GetPath(), *destination_storage_location_,
                           base::DoNothing());
  }
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
          &IsolatedWebAppUpdatePrepareAndStoreCommand::CopyToProfileDirectory,
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

  installed_version_ = installed_app->isolation_data()->version;
  GetMutableDebugValue().Set("installed_version",
                             installed_version_->GetString());
  if (expected_version_.has_value() &&
      *expected_version_ <= *installed_version_) {
    ReportFailure(base::StrCat({"Installed app is already on version ",
                                installed_version_->GetString(),
                                ". Cannot update to version ",
                                expected_version_->GetString()}));
    return;
  }

  if (installed_app->isolation_data()->location.dev_mode() !=
      update_source_->dev_mode()) {
    std::stringstream s;
    s << "Unable to update between dev-mode and non-dev-mode storage location "
         "types ("
      << installed_app->isolation_data()->location << " to " << *update_source_
      << ").";
    ReportFailure(s.str());
    return;
  }

  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CopyToProfileDirectory(
    base::OnceClosure next_step_callback) {
  UpdateBundlePathAndCreateStorageLocation(
      profile().GetPath(), *update_source_,
      base::BindOnce(&IsolatedWebAppUpdatePrepareAndStoreCommand::
                         OnCopiedToProfileDirectory,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::OnCopiedToProfileDirectory(
    base::OnceClosure next_step_callback,
    base::expected<IsolatedWebAppStorageLocation, std::string> new_location) {
  ASSIGN_OR_RETURN(destination_storage_location_, new_location,
                   &IsolatedWebAppUpdatePrepareAndStoreCommand::ReportFailure,
                   this);
  destination_location_ = IwaSourceWithMode::FromStorageLocation(
      profile().GetPath(), *destination_storage_location_);
  // Make sure that `update_source_`, which is now outdated, can no longer
  // be accessed.
  update_source_.reset();

  GetMutableDebugValue().Set("destination_location",
                             destination_location_->ToDebugValue());
  GetMutableDebugValue().Set("destination_storage_location",
                             destination_storage_location_->ToDebugValue());
  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  command_helper_->CheckTrustAndSignatures(
      *destination_location_, &profile(),
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
      *destination_location_, *web_contents_.get(), *url_loader_.get(),
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
      command_helper_->ValidateManifestAndCreateInstallInfo(expected_version_,
                                                            manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::
    RetrieveIconsAndPopulateInstallInfo(
        base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
        WebAppInstallInfo install_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(install_info.isolated_web_app_version.IsValid());
  if (expected_version_.has_value()) {
    CHECK_EQ(*expected_version_, install_info.isolated_web_app_version);
  }

  if (install_info.isolated_web_app_version <= *installed_version_) {
    ReportFailure(base::StrCat(
        {"Installed app is already on version ",
         installed_version_->GetString(), ". Cannot update to version ",
         install_info.isolated_web_app_version.GetString()}));
    return;
  }

  GetMutableDebugValue().Set("app_title", install_info.title);

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
  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(base::BindOnce(
      &IsolatedWebAppUpdatePrepareAndStoreCommand::OnFinalized,
      weak_factory_.GetWeakPtr(), info.isolated_web_app_version));

  WebApp* app_to_update = update->UpdateApp(url_info_.app_id());
  CHECK(app_to_update);

  WebApp::IsolationData updated_isolation_data =
      *app_to_update->isolation_data();
  updated_isolation_data.SetPendingUpdateInfo(
      WebApp::IsolationData::PendingUpdateInfo(*destination_storage_location_,
                                               info.isolated_web_app_version));
  app_to_update->SetIsolationData(std::move(updated_isolation_data));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::OnFinalized(
    const base::Version& update_version,
    bool success) {
  if (success) {
    ReportSuccess(update_version);
  } else {
    ReportFailure("Failed to save pending update info to Web App Database.");
  }
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportFailure(
    base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IsolatedWebAppUpdatePrepareAndStoreCommandError error{
      .message = std::string(message)};
  GetMutableDebugValue().Set("result", "error: " + error.message);
  CompleteAndSelfDestruct(CommandResult::kFailure,
                          base::unexpected(std::move(error)));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportSuccess(
    const base::Version& update_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reset `destination_storage_location_` to prevent cleanup in the destructor.
  auto destination_storage_location =
      std::exchange(destination_storage_location_, std::nullopt).value();
  CompleteAndSelfDestruct(CommandResult::kSuccess,
                          IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
                              update_version, destination_storage_location));
}

Profile& IsolatedWebAppUpdatePrepareAndStoreCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::UpdateInfo(
    IwaSourceWithModeAndFileOp source,
    std::optional<base::Version> expected_version)
    : source_(std::move(source)),
      expected_version_(std::move(expected_version)) {}

IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::~UpdateInfo() = default;

IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::UpdateInfo(
    const UpdateInfo&) = default;

IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo&
IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::operator=(
    const UpdateInfo&) = default;

base::Value
IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::AsDebugValue() const {
  return base::Value(
      base::Value::Dict()
          .Set("source", source_.ToDebugValue())
          .Set("expected_version", expected_version_.has_value()
                                       ? expected_version_->GetString()
                                       : "<any>"));
}

}  // namespace web_app
