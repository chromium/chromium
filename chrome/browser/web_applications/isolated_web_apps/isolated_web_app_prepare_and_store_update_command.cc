// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

IsolatedWebAppUpdatePrepareAndStoreCommandSuccess::
    IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
        base::Version update_version,
        IsolatedWebAppLocation destination_location)
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
    : WebAppCommandTemplate<AppLock>(
          "IsolatedWebAppUpdatePrepareAndStoreCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(url_info.app_id())),
      source_update_info_(std::move(update_info)),
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

  debug_log_ =
      base::Value::Dict()
          .Set("app_id", url_info_.app_id())
          .Set("origin", url_info_.origin().Serialize())
          .Set("bundle_id", url_info_.web_bundle_id().id())
          .Set("bundle_type",
               static_cast<int>(url_info_.web_bundle_id().type()))
          .Set("source_update_info", source_update_info_.AsDebugValue());
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
          &IsolatedWebAppUpdatePrepareAndStoreCommand::CopyToProfileDirectory,
          weak_ptr),
      base::BindOnce(
          &IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateLocation,
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
  debug_log_.Set("installed_version", installed_version_.GetString());
  if (source_update_info_.expected_version().has_value() &&
      *source_update_info_.expected_version() <= installed_version_) {
    ReportFailure(base::StrCat(
        {"Installed app is already on version ", installed_version_.GetString(),
         ". Cannot update to version ",
         source_update_info_.expected_version()->GetString()}));
    return;
  }
  if (installed_app->isolation_data()->location.index() !=
      source_update_info_.location().index()) {
    ReportFailure(
        base::StringPrintf("Unable to update between different "
                           "IsolatedWebAppLocation types (%zu to %zu).",
                           installed_app->isolation_data()->location.index(),
                           source_update_info_.location().index()));
    return;
  }

  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CopyToProfileDirectory(
    base::OnceCallback<void(base::expected<IsolatedWebAppLocation,
                                           std::string>)> next_step_callback) {
  CopyLocationToProfileDirectory(profile().GetPath(),
                                 source_update_info_.location(),
                                 std::move(next_step_callback));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateLocation(
    base::OnceClosure next_step_callback,
    base::expected<IsolatedWebAppLocation, std::string> new_location) {
  RETURN_IF_ERROR(new_location,
                  &IsolatedWebAppUpdatePrepareAndStoreCommand::ReportFailure,
                  this);
  lazy_destination_update_info_ = source_update_info_;
  lazy_destination_update_info_->set_location(std::move(*new_location));
  debug_log_.Set("lazy_destination_update_info",
                 lazy_destination_update_info_.value().AsDebugValue());
  std::move(next_step_callback).Run();
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(lazy_destination_update_info_);

  command_helper_->CheckTrustAndSignatures(
      lazy_destination_update_info_->location(), &profile(),
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
      lazy_destination_update_info_->location(), *web_contents_.get(),
      *url_loader_.get(),
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
          lazy_destination_update_info_->expected_version(), manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::
    RetrieveIconsAndPopulateInstallInfo(
        base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
        WebAppInstallInfo install_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(install_info.isolated_web_app_version.IsValid());
  if (lazy_destination_update_info_->expected_version().has_value()) {
    CHECK_EQ(lazy_destination_update_info_->expected_version().value(),
             install_info.isolated_web_app_version);
  }

  if (install_info.isolated_web_app_version <= installed_version_) {
    ReportFailure(base::StrCat(
        {"Installed app is already on version ", installed_version_.GetString(),
         ". Cannot update to version ",
         install_info.isolated_web_app_version.GetString()}));
    return;
  }

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
  ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate(base::BindOnce(
      &IsolatedWebAppUpdatePrepareAndStoreCommand::OnFinalized,
      weak_factory_.GetWeakPtr(), info.isolated_web_app_version));

  WebApp* app_to_update = update->UpdateApp(url_info_.app_id());
  CHECK(app_to_update);

  WebApp::IsolationData updated_isolation_data =
      *app_to_update->isolation_data();
  updated_isolation_data.SetPendingUpdateInfo(
      WebApp::IsolationData::PendingUpdateInfo(
          lazy_destination_update_info_->location(),
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

void IsolatedWebAppUpdatePrepareAndStoreCommand::OnShutdown() {
  // Stop any potential ongoing operations by destroying the `command_helper_`.
  command_helper_.reset();

  // TODO(cmfcmf): Test cancellation of pending update during system
  // shutdown.
  ReportFailure("System is shutting down.");
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportFailure(
    base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  if (lazy_destination_update_info_.has_value()) {
    CleanupLocationIfOwned(profile().GetPath(),
                           lazy_destination_update_info_->location(),
                           base::DoNothing());
  }

  IsolatedWebAppUpdatePrepareAndStoreCommandError error{
      .message = std::string(message)};
  debug_log_.Set("result", "error: " + error.message);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_), base::unexpected(std::move(error))));
}

void IsolatedWebAppUpdatePrepareAndStoreCommand::ReportSuccess(
    const base::Version& update_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!callback_.is_null());

  debug_log_.Set("result", "success");
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_),
                     IsolatedWebAppUpdatePrepareAndStoreCommandSuccess(
                         update_version,
                         lazy_destination_update_info_.value().location())));
}

Profile& IsolatedWebAppUpdatePrepareAndStoreCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo::UpdateInfo(
    IsolatedWebAppLocation location,
    absl::optional<base::Version> expected_version)
    : location_(std::move(location)),
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
          .Set("location", IsolatedWebAppLocationAsDebugValue(location_))
          .Set("expected_version", expected_version_.has_value()
                                       ? expected_version_->GetString()
                                       : "<any>"));
}

}  // namespace web_app
