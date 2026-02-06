// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_

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
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
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

enum class IconsDownloadedResult;
class IwaSourceWithModeAndFileOp;
class WebAppDataRetriever;
class WebAppRegistrar;

enum class VersionChangeValidationResult {
  kSameVersionUpdateDisallowed,
  kDowngradeDisallowed,
  kAllowed
};

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

// Provides the key rotation data associated with a particular IWA.
struct KeyRotationData {
  base::raw_span<const uint8_t> rotated_key;

  // Tells whether the current app installation contains the rotated key
  // (`iwa.isolation_data.integrity_block_data`).
  bool current_installation_has_rk;

  // Tells whether the pending update (if any) for the app contains the rotated
  // key (`iwa.isolation_data.pending_update_info.integrity_block_data`).
  bool pending_update_has_rk;
};

// Computes the key rotation data for `web_bundle_id` wrt rules above. Will
// return `std::nullopt` if there's no key rotation entry for this
// `web_bundle_id`.
std::optional<KeyRotationData> GetKeyRotationData(
    const web_package::SignedWebBundleId& web_bundle_id,
    const IsolationData& isolation_data);

// Checks if version change is allowed for given arguments.
VersionChangeValidationResult ValidateVersionChangeFeasibility(
    const IwaVersion& expected_version,
    const IwaVersion& installed_version,
    bool allow_downgrades,
    bool same_version_update_allowed_by_key_rotation);

// This is a helper class that contains methods which are shared between both
// install and update commands.
class IsolatedWebAppInstallCommandHelper {
 public:
  static std::unique_ptr<content::WebContents> CreateIsolatedWebAppWebContents(
      Profile& profile);

  IsolatedWebAppInstallCommandHelper(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<WebAppDataRetriever> data_retriever);
  ~IsolatedWebAppInstallCommandHelper();

  IsolatedWebAppInstallCommandHelper(
      const IsolatedWebAppInstallCommandHelper&) = delete;
  IsolatedWebAppInstallCommandHelper& operator=(
      const IsolatedWebAppInstallCommandHelper&) = delete;

  // Checks trust and signatures of IWA.
  // Returns the integrity block if the IWA is backed by a signed web bundle.
  void CheckTrustAndSignatures(
      const IwaSourceWithMode& location,
      const IwaOperation& operation,
      Profile* profile,
      base::OnceCallback<
          void(base::expected<
               std::optional<web_package::SignedWebBundleIntegrityBlock>,
               std::string>)> callback);

  // Checks trust and signatures of IWA.
  // Use this overload if you don't need the returned integrity block.
  void CheckTrustAndSignatures(
      const IwaSourceWithMode& location,
      const IwaOperation& operation,
      Profile* profile,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CreateStoragePartitionIfNotPresent(Profile& profile);

  void LoadInstallUrl(
      const IwaSourceWithMode& source,
      const IwaOperation& operation,
      content::WebContents& web_contents,
      webapps::WebAppUrlLoader& url_loader,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents& web_contents,
      base::OnceCallback<void(
          base::expected<blink::mojom::ManifestPtr, std::string>)> callback);

  base::expected<IwaVersion, std::string> ValidateManifestAndGetVersion(
      const std::optional<IwaVersion>& expected_version,
      const blink::mojom::Manifest& manifest);

  void RetrieveInstallInfoWithIconsFromManifest(
      const blink::mojom::Manifest& manifest,
      content::WebContents& web_contents,
      IwaVersion parsed_version,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback);

 private:
  void OnLoadInstallUrl(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      webapps::WebAppUrlLoaderResult result);

  void OnCheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(
          base::expected<blink::mojom::ManifestPtr, std::string>)> callback,
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void OnGettingInstallInfoFromManifest(
      IwaVersion parsed_version,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback,
      std::unique_ptr<WebAppInstallInfo> install_info);

  IsolatedWebAppUrlInfo url_info_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<ManifestToWebAppInstallInfoJob> manifest_to_install_info_job_;
  base::DictValue manifest_to_info_debug_data_;

  base::WeakPtrFactory<IsolatedWebAppInstallCommandHelper> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_COMMANDS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
