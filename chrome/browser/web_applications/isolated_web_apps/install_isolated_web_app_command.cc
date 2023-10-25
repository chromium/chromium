// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

InstallIsolatedWebAppCommandSuccess::InstallIsolatedWebAppCommandSuccess(
    base::Version installed_version,
    IsolatedWebAppLocation location)
    : installed_version(std::move(installed_version)),
      location(std::move(location)) {}

InstallIsolatedWebAppCommandSuccess::~InstallIsolatedWebAppCommandSuccess() =
    default;

InstallIsolatedWebAppCommandSuccess::InstallIsolatedWebAppCommandSuccess(
    const InstallIsolatedWebAppCommandSuccess& other) = default;

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandSuccess& success) {
  return os << "InstallIsolatedWebAppCommandSuccess { installed_version = \""
            << success.installed_version.GetString() << "\" }.";
}

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandError& error) {
  return os << "InstallIsolatedWebAppCommandError { message = \""
            << error.message << "\" }.";
}

InstallIsolatedWebAppCommand::InstallIsolatedWebAppCommand(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppLocation& location,
    const absl::optional<base::Version>& expected_version,
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(base::expected<InstallIsolatedWebAppCommandSuccess,
                                           InstallIsolatedWebAppCommandError>)>
        callback,
    std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommandTemplate<AppLock>("InstallIsolatedWebAppCommand"),
      lock_description_(
          std::make_unique<AppLockDescription>(url_info.app_id())),
      command_helper_(std::move(command_helper)),
      url_info_(url_info),
      source_location_(location),
      expected_version_(expected_version),
      web_contents_(std::move(web_contents)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  CHECK(web_contents_ != nullptr);
  CHECK(!callback.is_null());
  CHECK(optional_profile_keep_alive_ == nullptr ||
        &profile() == optional_profile_keep_alive_->profile());

  callback_ =
      base::BindOnce(
          [](base::expected<InstallIsolatedWebAppCommandSuccess,
                            InstallIsolatedWebAppCommandError> result) {
            webapps::InstallableMetrics::TrackInstallResult(result.has_value());
            return result;
          })
          .Then(std::move(callback));

  debug_log_ = base::Value::Dict()
                   .Set("app_id", url_info_.app_id())
                   .Set("origin", url_info_.origin().Serialize())
                   .Set("bundle_id", url_info_.web_bundle_id().id())
                   .Set("bundle_type",
                        static_cast<int>(url_info_.web_bundle_id().type()))
                   .Set("source_location",
                        IsolatedWebAppLocationAsDebugValue(source_location_))
                   .Set("expected_version", expected_version_.has_value()
                                                ? expected_version_->GetString()
                                                : "unknown");
}

InstallIsolatedWebAppCommand::~InstallIsolatedWebAppCommand() = default;

const LockDescription& InstallIsolatedWebAppCommand::lock_description() const {
  return *lock_description_;
}

base::Value InstallIsolatedWebAppCommand::ToDebugValue() const {
  return base::Value(debug_log_.Clone());
}

void InstallIsolatedWebAppCommand::StartWithLock(
    std::unique_ptr<AppLock> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lock_ = std::move(lock);
  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&InstallIsolatedWebAppCommand::CopyToProfileDirectory,
                     weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::UpdateLocation, weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::CheckTrustAndSignatures,
                     weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::CreateStoragePartition,
                     weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::LoadInstallUrl, weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::CheckInstallabilityAndRetrieveManifest,
          weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::ValidateManifestAndCreateInstallInfo,
          weak_ptr),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::RetrieveIconsAndPopulateInstallInfo,
          weak_ptr),
      base::BindOnce(&InstallIsolatedWebAppCommand::FinalizeInstall, weak_ptr));
}

void InstallIsolatedWebAppCommand::CopyToProfileDirectory(
    base::OnceCallback<void(base::expected<IsolatedWebAppLocation,
                                           std::string>)> next_step_callback) {
  CopyLocationToProfileDirectory(profile().GetPath(), source_location_,
                                 std::move(next_step_callback));
}

void InstallIsolatedWebAppCommand::UpdateLocation(
    base::OnceClosure next_step_callback,
    base::expected<IsolatedWebAppLocation, std::string> new_location) {
  ASSIGN_OR_RETURN(lazy_destination_location_, new_location,
                   &InstallIsolatedWebAppCommand::ReportFailure, this);

  debug_log_.Set(
      "lazy_destination_location",
      IsolatedWebAppLocationAsDebugValue(lazy_destination_location_.value()));
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::CheckTrustAndSignatures(
    base::OnceClosure next_step_callback) {
  CHECK(lazy_destination_location_);
  command_helper_->CheckTrustAndSignatures(
      *lazy_destination_location_, &profile(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  CHECK(lazy_destination_location_);
  command_helper_->LoadInstallUrl(
      *lazy_destination_location_, *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
        next_step_callback) {
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         IsolatedWebAppInstallCommandHelper::ManifestAndUrl>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::ValidateManifestAndCreateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url) {
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(expected_version_,
                                                            manifest_and_url);
  RunNextStepOnSuccess(std::move(next_step_callback), std::move(install_info));
}

void InstallIsolatedWebAppCommand::RetrieveIconsAndPopulateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    WebAppInstallInfo install_info) {
  CHECK(!expected_version_ ||
        *expected_version_ == install_info.isolated_web_app_version);
  actual_version_ = install_info.isolated_web_app_version;
  debug_log_.Set("actual_version", actual_version_.GetString());

  command_helper_->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         WebAppInstallInfo>,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::FinalizeInstall(WebAppInstallInfo info) {
  WebAppInstallFinalizer::FinalizeOptions options(
      webapps::WebappInstallSource::ISOLATED_APP_DEV_INSTALL);
  CHECK(lazy_destination_location_);
  options.isolated_web_app_location = *lazy_destination_location_;

  lock_->install_finalizer().FinalizeInstall(
      info, options,
      base::BindOnce(&InstallIsolatedWebAppCommand::OnFinalizeInstall,
                     weak_factory_.GetWeakPtr()));
}

void InstallIsolatedWebAppCommand::OnFinalizeInstall(
    const webapps::AppId& unused_app_id,
    webapps::InstallResultCode install_result_code,
    OsHooksErrors unused_os_hooks_errors) {
  if (install_result_code == webapps::InstallResultCode::kSuccessNewInstall) {
    ReportSuccess();
  } else {
    std::stringstream os;
    os << "Error during finalization: " << install_result_code;
    ReportFailure(os.str());
  }
}

void InstallIsolatedWebAppCommand::OnShutdown() {
  // Stop any potential ongoing operations by destroying the `command_helper_`.
  command_helper_.reset();

  // TODO(cmfcmf): Test cancellation of pending installation during system
  // shutdown.
  ReportFailure("System is shutting down.");
}

void InstallIsolatedWebAppCommand::ReportFailure(base::StringPiece message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  if (lazy_destination_location_.has_value()) {
    CleanupLocationIfOwned(profile().GetPath(),
                           lazy_destination_location_.value(),
                           base::DoNothing());
  }

  debug_log_.Set("result", base::StrCat({"error: ", message}));
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(callback_),
                     base::unexpected(InstallIsolatedWebAppCommandError{
                         .message = std::string(message)})));
}

void InstallIsolatedWebAppCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());

  debug_log_.Set("result", "success");
  SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess,
      base::BindOnce(std::move(callback_),
                     InstallIsolatedWebAppCommandSuccess(
                         actual_version_, lazy_destination_location_.value())));
}

Profile& InstallIsolatedWebAppCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
