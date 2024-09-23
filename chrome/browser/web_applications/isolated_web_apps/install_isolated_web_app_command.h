// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

struct InstallIsolatedWebAppCommandSuccess {
  InstallIsolatedWebAppCommandSuccess(IsolatedWebAppUrlInfo url_info,
                                      base::Version installed_version,
                                      IsolatedWebAppStorageLocation location);
  InstallIsolatedWebAppCommandSuccess(
      const InstallIsolatedWebAppCommandSuccess& other);
  ~InstallIsolatedWebAppCommandSuccess();

  IsolatedWebAppUrlInfo url_info;
  base::Version installed_version;
  IsolatedWebAppStorageLocation location;
};

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandSuccess& success);

struct InstallIsolatedWebAppCommandError {
  std::string message;
};

std::ostream& operator<<(std::ostream& os,
                         const InstallIsolatedWebAppCommandError& error);

// Isolated Web App requires:
//  * no cross-origin navigation
//  * content should never be loaded in normal tab
//
// |content::IsolatedWebAppThrottle| enforces that. The requirements prevent
// re-using web contents.
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
      const std::optional<base::Version>& expected_version,
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

  void ReportFailure(InstallIwaError error, std::string_view message);
  void ReportSuccess();

  template <typename T, std::enable_if_t<std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceClosure next_step_callback,
                            InstallIwaError error,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(error, status.error());
    } else {
      std::move(next_step_callback).Run();
    }
  }

  template <typename T, std::enable_if_t<!std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceCallback<void(T)> next_step_callback,
                            InstallIwaError error,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(error, status.error());
    } else {
      std::move(next_step_callback).Run(std::move(*status));
    }
  }

  Profile& profile();

  void CopyToProfileDirectory(base::OnceClosure next_step_callback);

  void OnCopiedToProfileDirectory(
      base::OnceClosure next_step_callback,
      base::expected<IsolatedWebAppStorageLocation, std::string> new_location);

  void CheckTrustAndSignatures(
      base::OnceCallback<
          void(std::optional<web_package::SignedWebBundleIntegrityBlock>)>
          next_step_callback);

  void CreateStoragePartition(
      base::OnceClosure next_step_callback,
      std::optional<web_package::SignedWebBundleIntegrityBlock>
          integrity_block);

  void LoadInstallUrl(base::OnceClosure next_step_callback);

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(blink::mojom::ManifestPtr)> next_step_callback);

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      blink::mojom::ManifestPtr manifest);

  void RetrieveIconsAndPopulateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      WebAppInstallInfo install_info);

  void FinalizeInstall(WebAppInstallInfo info);
  void OnFinalizeInstall(const webapps::AppId& unused_app_id,
                         webapps::InstallResultCode install_result_code);

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<AppLock> lock_;
  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;

  const std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  const IsolatedWebAppUrlInfo url_info_;
  const std::optional<base::Version> expected_version_;
  const webapps::WebappInstallSource install_surface_;

  std::optional<web_package::SignedWebBundleIntegrityBlock> integrity_block_;

  std::optional<IwaSourceWithModeAndFileOp> install_source_;
  std::optional<IwaSourceWithMode> destination_source_;
  std::optional<IsolatedWebAppStorageLocation> destination_storage_location_;
  std::optional<base::Version> actual_version_;

  std::unique_ptr<content::WebContents> web_contents_;

  const std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  const std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  base::WeakPtrFactory<InstallIsolatedWebAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
