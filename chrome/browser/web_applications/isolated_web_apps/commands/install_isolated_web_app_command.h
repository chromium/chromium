// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/prepare_install_info_job.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_factory.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

// Represents a successful installation of an Isolated Web App.
struct InstallIsolatedWebAppCommandSuccess {
  InstallIsolatedWebAppCommandSuccess(IsolatedWebAppUrlInfo url_info,
                                      IwaVersion installed_version,
                                      IsolatedWebAppStorageLocation location);
  InstallIsolatedWebAppCommandSuccess(
      const InstallIsolatedWebAppCommandSuccess& other);
  ~InstallIsolatedWebAppCommandSuccess();

  IsolatedWebAppUrlInfo url_info;
  IwaVersion installed_version;
  IsolatedWebAppStorageLocation location;
};

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandSuccess& success);

// Represents an error during the installation of an Isolated Web App.
struct InstallIsolatedWebAppCommandError {
  std::string message;
};

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandError& error);

// Command to install an Isolated Web App from a given `IsolatedWebAppUrlInfo`
// and `IsolatedWebAppInstallSource`. This command will perform a full
// installation, including checking for trust and signatures, creating the
// storage partition, and finalizing the installation.
class InstallIsolatedWebAppCommand
    : public WebAppCommand<AppLock,
                           base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>> {
 public:
  // `url_info` holds the origin information of the app. It is randomly
  // generated for dev-proxy and the public key of signed bundle. It is
  // guarantee to be valid.
  //
  // `location` holds information about the mode(dev-mod-proxy/signed-bundle)
  // and the source.
  //
  // `expected_version`, if set, specifies the expected version of the IWA to
  // install. If the version in the manifest differs, install is aborted.
  //
  // `callback` must be not null.
  //
  // The `id` in the application's manifest must equal "/".
  //
  // `response_reader_factory` should be created via
  // `CreateDefaultResponseReaderFactory` and is used to create the
  // `IsolatedWebAppResponseReader` for the Web Bundle.
  InstallIsolatedWebAppCommand(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppInstallSource& install_source,
      const std::optional<IwaVersion>& expected_version,
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      base::OnceCallback<
          void(base::expected<InstallIsolatedWebAppCommandSuccess,
                              InstallIsolatedWebAppCommandError>)> callback,
      std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper);

  InstallIsolatedWebAppCommand(const InstallIsolatedWebAppCommand&) = delete;
  InstallIsolatedWebAppCommand& operator=(const InstallIsolatedWebAppCommand&) =
      delete;

  InstallIsolatedWebAppCommand(InstallIsolatedWebAppCommand&&) = delete;
  InstallIsolatedWebAppCommand& operator=(InstallIsolatedWebAppCommand&&) =
      delete;

  ~InstallIsolatedWebAppCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;

 private:
  using TrustCheckResult =
      base::expected<std::optional<web_package::SignedWebBundleIntegrityBlock>,
                     std::string>;

  // This enum lists the error types that can occur during the installation of
  // an isolated web apps.
  //
  // These values are persisted to logs and the values match the entries of
  // `enum IsolatedWebAppInstallError` in
  // `tools/metrics/histograms/metadata/webapps/enums.xml`.
  // Entries should not be renumbered and numeric values should never be reused.
  enum class InstallIwaError {
    kCantCopyToProfileDirectory = 1,
    kTrustCheckFailed = 2,
    kCantLoadInstallUrl = 3,
    kAppIsNotInstallable = 4,
    kCantValidateManifest = 5,
    kCantRetrieveIcons = 6,
    kCantInstall = 7,
    kMaxValue = kCantInstall
  };

  void ReportFailure(InstallIwaError error,
                     webapps::InstallResultCode web_app_failure_code,
                     std::string_view message);
  void ReportSuccess(const IwaVersion& installed_version);

  Profile& profile();

  void CheckNotInstalledAlready(base::OnceClosure next_step_callback);

  void CopyToProfileDirectory(base::OnceClosure next_step_callback);

  void OnCopiedToProfileDirectory(
      base::OnceClosure next_step_callback,
      base::expected<IsolatedWebAppStorageLocation, std::string> new_location);

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback);

  void OnTrustAndSignaturesChecked(base::OnceClosure next_step_callback,
                                   TrustCheckResult trust_check_result);

  void CreateStoragePartition(base::OnceClosure next_step_callback);

  void PrepareInstallInfo(
      base::OnceCallback<void(PrepareInstallInfoJob::InstallInfoOrFailure)>
          next_step_callback);

  void FinalizeInstall(PrepareInstallInfoJob::InstallInfoOrFailure result);

  void OnFinalizeInstall(const IwaVersion& attempted_version,
                         const webapps::AppId& unused_app_id,
                         webapps::InstallResultCode install_result_code);

  std::unique_ptr<AppLock> lock_;

  const std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  const IsolatedWebAppUrlInfo url_info_;
  const std::optional<IwaVersion> expected_version_;
  const webapps::WebappInstallSource install_surface_;

  std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data_;

  std::optional<IwaSourceWithModeAndFileOp> install_source_;
  std::optional<IwaSourceWithMode> destination_source_;
  std::optional<IsolatedWebAppStorageLocation> destination_storage_location_;

  std::unique_ptr<content::WebContents> web_contents_;

  const std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  const std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  std::unique_ptr<PrepareInstallInfoJob> prepare_install_info_job_;

  base::WeakPtrFactory<InstallIsolatedWebAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
