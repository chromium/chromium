// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_response_reader_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppDataRetriever;
class WebAppUrlLoader;

enum class WebAppUrlLoaderResult;

struct InstallIsolatedWebAppCommandSuccess {
  InstallIsolatedWebAppCommandSuccess(base::Version installed_version,
                                      IsolatedWebAppLocation location);
  InstallIsolatedWebAppCommandSuccess(
      const InstallIsolatedWebAppCommandSuccess& other);
  ~InstallIsolatedWebAppCommandSuccess();

  base::Version installed_version;
  IsolatedWebAppLocation location;
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
class InstallIsolatedWebAppCommand : public WebAppCommandTemplate<AppLock> {
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
  explicit InstallIsolatedWebAppCommand(
      const IsolatedWebAppUrlInfo& url_info,
      const IsolatedWebAppLocation& location,
      const absl::optional<base::Version>& expected_version,
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

  // WebAppCommandTemplate<AppLock>:
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  void OnShutdown() override;

  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void ReportFailure(base::StringPiece message);
  void ReportSuccess();

  template <typename T, std::enable_if_t<std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceClosure next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(status.error());
    } else {
      std::move(next_step_callback).Run();
    }
  }

  template <typename T, std::enable_if_t<!std::is_void_v<T>, bool> = true>
  void RunNextStepOnSuccess(base::OnceCallback<void(T)> next_step_callback,
                            base::expected<T, std::string> status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!status.has_value()) {
      ReportFailure(status.error());
    } else {
      std::move(next_step_callback).Run(std::move(*status));
    }
  }

  Profile& profile();

  void CopyToProfileDirectory(
      base::OnceCallback<void(base::expected<IsolatedWebAppLocation,
                                             std::string>)> next_step_callback);

  void UpdateLocation(
      base::OnceClosure next_step_callback,
      base::expected<IsolatedWebAppLocation, std::string> new_location);

  void CheckTrustAndSignatures(base::OnceClosure next_step_callback);

  void CreateStoragePartition(base::OnceClosure next_step_callback);

  void LoadInstallUrl(base::OnceClosure next_step_callback);

  void CheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<
          void(IsolatedWebAppInstallCommandHelper::ManifestAndUrl)>
          next_step_callback);

  void ValidateManifestAndCreateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      IsolatedWebAppInstallCommandHelper::ManifestAndUrl manifest_and_url);

  void RetrieveIconsAndPopulateInstallInfo(
      base::OnceCallback<void(WebAppInstallInfo)> next_step_callback,
      WebAppInstallInfo install_info);

  void FinalizeInstall(WebAppInstallInfo info);
  void OnFinalizeInstall(const webapps::AppId& unused_app_id,
                         webapps::InstallResultCode install_result_code,
                         OsHooksErrors unused_os_hooks_errors);

  SEQUENCE_CHECKER(sequence_checker_);

  base::Value::Dict debug_log_;
  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;
  std::unique_ptr<WebAppUrlLoader> url_loader_;

  std::unique_ptr<IsolatedWebAppInstallCommandHelper> command_helper_;

  IsolatedWebAppUrlInfo url_info_;
  IsolatedWebAppLocation source_location_;
  absl::optional<IsolatedWebAppLocation> lazy_destination_location_;

  absl::optional<base::Version> expected_version_;
  // Populated as part of the installation process based on the version read
  // from the Web Bundle.
  base::Version actual_version_;

  std::unique_ptr<content::WebContents> web_contents_;

  std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;

  base::OnceCallback<void(base::expected<InstallIsolatedWebAppCommandSuccess,
                                         InstallIsolatedWebAppCommandError>)>
      callback_;

  base::WeakPtrFactory<InstallIsolatedWebAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_COMMAND_H_
