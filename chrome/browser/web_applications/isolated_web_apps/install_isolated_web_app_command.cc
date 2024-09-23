// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/strings/to_string.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/error/uma_logging.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

InstallIsolatedWebAppCommandSuccess::InstallIsolatedWebAppCommandSuccess(
    IsolatedWebAppUrlInfo url_info,
    base::Version installed_version,
    IsolatedWebAppStorageLocation location)
    : url_info(std::move(url_info)),
      installed_version(std::move(installed_version)),
      location(std::move(location)) {}

InstallIsolatedWebAppCommandSuccess::~InstallIsolatedWebAppCommandSuccess() =
    default;

InstallIsolatedWebAppCommandSuccess::InstallIsolatedWebAppCommandSuccess(
    const InstallIsolatedWebAppCommandSuccess& other) = default;

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandSuccess& success) {
  return os << "InstallIsolatedWebAppCommandSuccess "
            << base::Value::Dict()
                   .Set("installed_version",
                        success.installed_version.GetString())
                   .Set("location", base::ToString(success.location));
}

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandError& error) {
  return os << "InstallIsolatedWebAppCommandError { message = \""
            << error.message << "\" }.";
}

InstallIsolatedWebAppCommand::InstallIsolatedWebAppCommand(
    const IsolatedWebAppUrlInfo& url_info,
    const IsolatedWebAppInstallSource& install_source,
    const std::optional<base::Version>& expected_version,
    std::unique_ptr<content::WebContents> web_contents,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::OnceCallback<void(base::expected<InstallIsolatedWebAppCommandSuccess,
                                           InstallIsolatedWebAppCommandError>)>
        callback,
    std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper)
    : WebAppCommand<AppLock,
                    base::expected<InstallIsolatedWebAppCommandSuccess,
                                   InstallIsolatedWebAppCommandError>>(
          "InstallIsolatedWebAppCommand",
          AppLockDescription(url_info.app_id()),
          base::BindOnce(
              [](web_package::SignedWebBundleId web_bundle_id,
                 webapps::WebappInstallSource install_source,
                 base::expected<InstallIsolatedWebAppCommandSuccess,
                                InstallIsolatedWebAppCommandError> result) {
                webapps::InstallableMetrics::TrackInstallResult(
                    result.has_value(), install_source);
                DVLOG(0) << "Install result of IWA "
                         << base::ToString(web_bundle_id) << ": "
                         << (result.has_value()
                                 ? base::ToString(result.value())
                                 : base::ToString(result.error()));
                return result;
              },
              url_info.web_bundle_id(),
              install_source.install_surface())
              .Then(std::move(callback)),
          /*args_for_shutdown=*/
          base::unexpected(InstallIsolatedWebAppCommandError{
              .message = std::string("System shutting down.")})),
      command_helper_(std::move(command_helper)),
      url_info_(url_info),
      expected_version_(expected_version),
      install_surface_(install_source.install_surface()),
      install_source_(install_source.source()),
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
  GetMutableDebugValue().Set("install_surface",
                             base::ToString(install_surface_));
  GetMutableDebugValue().Set("install_source", install_source_->ToDebugValue());
  GetMutableDebugValue().Set("expected_version",
                             expected_version_.has_value()
                                 ? expected_version_->GetString()
                                 : "unknown");
}

InstallIsolatedWebAppCommand::~InstallIsolatedWebAppCommand() {
  if (destination_storage_location_.has_value()) {
    CleanupLocationIfOwned(profile().GetPath(), *destination_storage_location_,
                           base::DoNothing());
  }
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
    base::OnceClosure next_step_callback) {
  UpdateBundlePathAndCreateStorageLocation(
      profile().GetPath(), *install_source_,
      base::BindOnce(&InstallIsolatedWebAppCommand::OnCopiedToProfileDirectory,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::OnCopiedToProfileDirectory(
    base::OnceClosure next_step_callback,
    base::expected<IsolatedWebAppStorageLocation, std::string> new_location) {
  ASSIGN_OR_RETURN(destination_storage_location_, new_location,
                   &InstallIsolatedWebAppCommand::ReportFailure, this,
                   InstallIwaError::kCantCopyToProfileDirectory);
  destination_source_ = IwaSourceWithMode::FromStorageLocation(
      profile().GetPath(), *destination_storage_location_);
  // Make sure that `install_source_`, which is now outdated, can no longer be
  // accessed.
  install_source_.reset();

  GetMutableDebugValue().Set("destination_source",
                             destination_source_->ToDebugValue());
  GetMutableDebugValue().Set("destination_storage_location",
                             destination_storage_location_->ToDebugValue());
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::CheckTrustAndSignatures(
    base::OnceCallback<
        void(std::optional<web_package::SignedWebBundleIntegrityBlock>)>
        next_step_callback) {
  command_helper_->CheckTrustAndSignatures(
      *destination_source_, &profile(),
      base::BindOnce(
          &InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
              std::optional<web_package::SignedWebBundleIntegrityBlock>>,
          weak_factory_.GetWeakPtr(), std::move(next_step_callback),
          InstallIwaError::kTrustCheckFailed));
}

void InstallIsolatedWebAppCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback,
    std::optional<web_package::SignedWebBundleIntegrityBlock> integrity_block) {
  integrity_block_ = std::move(integrity_block);
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::LoadInstallUrl(
    base::OnceClosure next_step_callback) {
  command_helper_->LoadInstallUrl(
      *destination_source_, *web_contents_.get(), *url_loader_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<void>,
                     weak_factory_.GetWeakPtr(), std::move(next_step_callback),
                     InstallIwaError::kCantLoadInstallUrl));
}

void InstallIsolatedWebAppCommand::CheckInstallabilityAndRetrieveManifest(
    base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback) {
  command_helper_->CheckInstallabilityAndRetrieveManifest(
      *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         blink::mojom::ManifestPtr>,
                     weak_factory_.GetWeakPtr(), std::move(next_step_callback),
                     InstallIwaError::kAppIsNotInstallable));
}

void InstallIsolatedWebAppCommand::ValidateManifestAndCreateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    blink::mojom::ManifestPtr manifest) {
  base::expected<WebAppInstallInfo, std::string> install_info =
      command_helper_->ValidateManifestAndCreateInstallInfo(expected_version_,
                                                            *manifest);
  RunNextStepOnSuccess(std::move(next_step_callback),
                       InstallIwaError::kCantValidateManifest,
                       std::move(install_info));
}

void InstallIsolatedWebAppCommand::RetrieveIconsAndPopulateInstallInfo(
    base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
    WebAppInstallInfo install_info) {
  CHECK(!expected_version_ ||
        *expected_version_ == install_info.isolated_web_app_version);
  actual_version_ = install_info.isolated_web_app_version;
  GetMutableDebugValue().Set("actual_version", actual_version_->GetString());

  command_helper_->RetrieveIconsAndPopulateInstallInfo(
      std::move(install_info), *web_contents_.get(),
      base::BindOnce(&InstallIsolatedWebAppCommand::RunNextStepOnSuccess<
                         WebAppInstallInfo>,
                     weak_factory_.GetWeakPtr(), std::move(next_step_callback),
                     InstallIwaError::kCantRetrieveIcons));
}

void InstallIsolatedWebAppCommand::FinalizeInstall(WebAppInstallInfo info) {
  WebAppInstallFinalizer::FinalizeOptions options(install_surface_);

  options.iwa_options = WebAppInstallFinalizer::FinalizeOptions::IwaOptions(
      *destination_storage_location_,
      integrity_block_
          ? std::make_optional(
                IsolatedWebAppIntegrityBlockData::FromIntegrityBlock(
                    *integrity_block_))
          : std::nullopt);

  lock_->install_finalizer().FinalizeInstall(
      info, options,
      base::BindOnce(&InstallIsolatedWebAppCommand::OnFinalizeInstall,
                     weak_factory_.GetWeakPtr()));
}

void InstallIsolatedWebAppCommand::OnFinalizeInstall(
    const webapps::AppId& unused_app_id,
    webapps::InstallResultCode install_result_code) {
  if (install_result_code == webapps::InstallResultCode::kSuccessNewInstall) {
    ReportSuccess();
  } else {
    ReportFailure(
        InstallIwaError::kCantInstall,
        "Error during finalization: " + base::ToString(install_result_code));
  }
}

void InstallIsolatedWebAppCommand::ReportFailure(InstallIwaError error,
                                                 std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetMutableDebugValue().Set("result", base::StrCat({"error: ", message}));

  web_app::UmaLogExpectedStatus<InstallIwaError>("WebApp.Isolated.Install",
                                                 base::unexpected(error));

  CompleteAndSelfDestruct(CommandResult::kFailure,
                          base::unexpected(InstallIsolatedWebAppCommandError{
                              .message = std::string(message)}));
}

void InstallIsolatedWebAppCommand::ReportSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetMutableDebugValue().Set("result", "success");
  // Reset `destination_storage_location_` to prevent cleanup in the destructor.
  IsolatedWebAppStorageLocation location =
      std::exchange(destination_storage_location_, std::nullopt).value();

  web_app::UmaLogExpectedStatus<InstallIwaError>("WebApp.Isolated.Install",
                                                 base::ok());

  CompleteAndSelfDestruct(
      CommandResult::kSuccess,
      InstallIsolatedWebAppCommandSuccess(url_info_, *actual_version_,
                                          std::move(location)));
}

Profile& InstallIsolatedWebAppCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
