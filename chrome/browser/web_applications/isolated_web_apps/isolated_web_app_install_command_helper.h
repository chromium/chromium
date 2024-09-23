// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

enum class IconsDownloadedResult;
class IsolatedWebAppResponseReader;
class IsolatedWebAppStorageLocation;
class IsolatedWebAppResponseReaderFactory;
class IwaSourceWithMode;
class IwaSourceWithModeAndFileOp;
class UnusableSwbnFileError;
class WebAppDataRetriever;
class WebAppRegistrar;

// Copies the file being installed to the profile directory.
// On success returns a new owned location in the callback.
void UpdateBundlePathAndCreateStorageLocation(
    const base::FilePath& profile_dir,
    const IwaSourceWithModeAndFileOp& source,
    base::OnceCallback<void(
        base::expected<IsolatedWebAppStorageLocation, std::string>)> callback);

// Removes the IWA's randomly named directory in the profile directory.
// Calls the closure on complete.
void CleanupLocationIfOwned(const base::FilePath& profile_dir,
                            const IsolatedWebAppStorageLocation& location,
                            base::OnceClosure closure);

// Gets a web app from `registrar` for the given `iwa_id` and validates that
// it's a valid IWA (i.e. features `isolation_data`). Returns an error if
// there's no web app or `isolation_data` is missing.
base::expected<std::reference_wrapper<const WebApp>, std::string>
GetIsolatedWebAppById(const WebAppRegistrar& registrar,
                      const webapps::AppId& iwa_id);

base::flat_map<web_package::SignedWebBundleId,
               std::reference_wrapper<const WebApp>>
GetInstalledIwas(const WebAppRegistrar& registrar);

enum class KeyRotationLookupResult { kNoKeyRotation, kKeyFound, kKeyBlocked };

// Queries the `IwaKeyDistributionInfoProvider` whether there's
// `KeyRotationInfo` associated with the given `web_bundle_id`.
//   * If there's no key found, returns `kNoKeyRotation`.
//   * If the rotated key is null, reflects this in `debug_log` and returns
//     `kKeyBlocked`.
//   * Otherwise, writes the key data into `debug_log` and returns `kKeyFound.`
KeyRotationLookupResult LookupRotatedKey(
    const web_package::SignedWebBundleId& web_bundle_id,
    base::optional_ref<base::Value::Dict> debug_log = std::nullopt);

// Provides the key rotation data associated with a particular IWA.
struct KeyRotationData {
  const raw_ref<const std::vector<uint8_t>> rotated_key;

  // Tells whether the current app installation contains the rotated key
  // (`iwa.isolation_data.integrity_block_data`).
  bool current_installation_has_rk;

  // Tells whether the pending update (if any) for the app contains the rotated
  // key (`iwa.isolation_data.pending_update_info.integrity_block_data`).
  bool pending_update_has_rk;
};

// Computes the key rotation data as outlined above.
// This function must only be called if the result of invoking
// `LookupRotatedKey()` has yielded `kKeyFound` (will CHECK otherwise).
KeyRotationData GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data);

// This is a helper class that contains methods which are shared between both
// install and update commands.
class IsolatedWebAppInstallCommandHelper {
 public:
  static std::unique_ptr<IsolatedWebAppResponseReaderFactory>
  CreateDefaultResponseReaderFactory(Profile& profile);

  static std::unique_ptr<content::WebContents> CreateIsolatedWebAppWebContents(
      Profile& profile);

  IsolatedWebAppInstallCommandHelper(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      std::unique_ptr<IsolatedWebAppResponseReaderFactory>
          response_reader_factory);
  ~IsolatedWebAppInstallCommandHelper();

  IsolatedWebAppInstallCommandHelper(
      const IsolatedWebAppInstallCommandHelper&) = delete;
  IsolatedWebAppInstallCommandHelper& operator=(
      const IsolatedWebAppInstallCommandHelper&) = delete;

  // Checks trust and signatures of IWA.
  // Returns the integrity block if the IWA is backed by a signed web bundle.
  void CheckTrustAndSignatures(
      const IwaSourceWithMode& location,
      Profile* profile,
      base::OnceCallback<
          void(base::expected<
               std::optional<web_package::SignedWebBundleIntegrityBlock>,
               std::string>)> callback);

  // Checks trust and signatures of IWA.
  // Use this overload if you don't need the returned integrity block.
  void CheckTrustAndSignatures(
      const IwaSourceWithMode& location,
      Profile* profile,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CreateStoragePartitionIfNotPresent(Profile& profile);

  void LoadInstallUrl(
      const IwaSourceWithMode& source,
      content::WebContents& web_contents,
      webapps::WebAppUrlLoader& url_loader,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents& web_contents,
      base::OnceCallback<void(
          base::expected<blink::mojom::ManifestPtr, std::string>)> callback);

  base::expected<WebAppInstallInfo, std::string>
  ValidateManifestAndCreateInstallInfo(
      const std::optional<base::Version>& expected_version,
      const blink::mojom::Manifest& manifest);

  void RetrieveIconsAndPopulateInstallInfo(
      WebAppInstallInfo install_info,
      content::WebContents& web_contents,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback);

 private:
  void CheckTrustAndSignaturesOfBundle(
      const base::FilePath& path,
      bool dev_mode,
      base::OnceCallback<
          void(base::expected<
               std::optional<web_package::SignedWebBundleIntegrityBlock>,
               std::string>)> callback);

  void OnTrustAndSignaturesOfBundleChecked(
      base::OnceCallback<
          void(base::expected<
               std::optional<web_package::SignedWebBundleIntegrityBlock>,
               std::string>)> callback,
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError> status);

  void OnLoadInstallUrl(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      webapps::WebAppUrlLoaderResult result);

  void OnCheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(
          base::expected<blink::mojom::ManifestPtr, std::string>)> callback,
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void OnRetrieveIcons(
      WebAppInstallInfo install_info,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback,
      IconsDownloadedResult result,
      IconsMap icons_map,
      DownloadedIconsHttpResults unused_icons_http_results);

  IsolatedWebAppUrlInfo url_info_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<IsolatedWebAppResponseReaderFactory> response_reader_factory_;

  base::WeakPtrFactory<IsolatedWebAppInstallCommandHelper> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
