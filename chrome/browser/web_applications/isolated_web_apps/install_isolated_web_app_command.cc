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
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
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
  lock_ = std::move(lock);

  RunChainedWeakCallbacks(
      weak_factory_.GetWeakPtr(),
      &InstallIsolatedWebAppCommand::CheckNotInstalledAlready,
      &InstallIsolatedWebAppCommand::CopyToProfileDirectory,
      &InstallIsolatedWebAppCommand::CheckTrustAndSignatures,
      &InstallIsolatedWebAppCommand::CreateStoragePartition,
      &InstallIsolatedWebAppCommand::PrepareInstallInfo,
      &InstallIsolatedWebAppCommand::FinalizeInstall);
}

void InstallIsolatedWebAppCommand::CheckNotInstalledAlready(
    base::OnceClosure next_step_callback) {
  if (GetIsolatedWebAppById(lock_->registrar(), url_info_.app_id())
          .has_value()) {
    ReportFailure(InstallIwaError::kAppIsNotInstallable,
                  "App is already installed");
  } else {
    std::move(next_step_callback).Run();
  }
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
    base::OnceClosure next_step_callback) {
  command_helper_->CheckTrustAndSignatures(
      *destination_source_, &profile(),
      base::BindOnce(&InstallIsolatedWebAppCommand::OnTrustAndSignaturesChecked,
                     weak_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void InstallIsolatedWebAppCommand::OnTrustAndSignaturesChecked(
    base::OnceClosure next_step_callback,
    TrustCheckResult trust_check_result) {
  ASSIGN_OR_RETURN(
      std::optional<web_package::SignedWebBundleIntegrityBlock> integrity_block,
      std::move(trust_check_result), [&](const std::string& error) {
        ReportFailure(InstallIwaError::kTrustCheckFailed, error);
      });
  if (integrity_block) {
    integrity_block_data_ =
        IsolatedWebAppIntegrityBlockData::FromIntegrityBlock(*integrity_block);
  }
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::CreateStoragePartition(
    base::OnceClosure next_step_callback) {
  command_helper_->CreateStoragePartitionIfNotPresent(profile());
  std::move(next_step_callback).Run();
}

void InstallIsolatedWebAppCommand::PrepareInstallInfo(
    base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
        next_step_callback) {
  prepare_install_info_job_ = PrepareInstallInfoJob::CreateAndStart(
      profile(), *destination_source_, expected_version_, *web_contents_,
      *command_helper_, lock_->web_contents_manager().CreateUrlLoader(),
      std::move(next_step_callback));
}

void InstallIsolatedWebAppCommand::FinalizeInstall(
    PrepareInstallInfoJob::InstallInfoOrFailure result) {
  prepare_install_info_job_.reset();

  ASSIGN_OR_RETURN(
      WebAppInstallInfo install_info, std::move(result),
      [&](const auto& failure) {
        auto iwa_error = [&] {
          switch (failure.error) {
            case PrepareInstallInfoJob::Error::kAppIsNotInstallable:
              return InstallIwaError::kAppIsNotInstallable;
            case PrepareInstallInfoJob::Error::kCantLoadInstallUrl:
              return InstallIwaError::kCantLoadInstallUrl;
            case PrepareInstallInfoJob::Error::kCantRetrieveIcons:
              return InstallIwaError::kCantRetrieveIcons;
            case PrepareInstallInfoJob::Error::kCantValidateManifest:
              return InstallIwaError::kCantValidateManifest;
          }
        }();
        ReportFailure(iwa_error, failure.message);
      });

  GetMutableDebugValue().Set("actual_version",
                             install_info.isolated_web_app_version.GetString());
  GetMutableDebugValue().Set("app_title", install_info.title);

  WebAppInstallFinalizer::FinalizeOptions options(install_surface_);

  options.iwa_options = WebAppInstallFinalizer::FinalizeOptions::IwaOptions(
      *destination_storage_location_, std::move(integrity_block_data_));

  lock_->install_finalizer().FinalizeInstall(
      install_info, options,
      base::BindOnce(&InstallIsolatedWebAppCommand::OnFinalizeInstall,
                     weak_factory_.GetWeakPtr(),
                     install_info.isolated_web_app_version));
}

void InstallIsolatedWebAppCommand::OnFinalizeInstall(
    const base::Version& attempted_version,
    const webapps::AppId& unused_app_id,
    webapps::InstallResultCode install_result_code) {
  if (install_result_code == webapps::InstallResultCode::kSuccessNewInstall) {
    ReportSuccess(attempted_version);
  } else {
    ReportFailure(
        InstallIwaError::kCantInstall,
        "Error during finalization: " + base::ToString(install_result_code));
  }
}

void InstallIsolatedWebAppCommand::ReportFailure(InstallIwaError error,
                                                 std::string_view message) {
  GetMutableDebugValue().Set("result", base::StrCat({"error: ", message}));

  web_app::UmaLogExpectedStatus<InstallIwaError>("WebApp.Isolated.Install",
                                                 base::unexpected(error));

  CompleteAndSelfDestruct(CommandResult::kFailure,
                          base::unexpected(InstallIsolatedWebAppCommandError{
                              .message = std::string(message)}));
}

void InstallIsolatedWebAppCommand::ReportSuccess(
    const base::Version& installed_version) {
  GetMutableDebugValue().Set("result", "success");
  // Reset `destination_storage_location_` to prevent cleanup in the destructor.
  IsolatedWebAppStorageLocation location =
      std::exchange(destination_storage_location_, std::nullopt).value();

  web_app::UmaLogExpectedStatus<InstallIwaError>("WebApp.Isolated.Install",
                                                 base::ok());

  CompleteAndSelfDestruct(
      CommandResult::kSuccess,
      InstallIsolatedWebAppCommandSuccess(url_info_, installed_version,
                                          std::move(location)));
}

Profile& InstallIsolatedWebAppCommand::profile() {
  CHECK(web_contents_);
  CHECK(web_contents_->GetBrowserContext());
  return *Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace web_app
