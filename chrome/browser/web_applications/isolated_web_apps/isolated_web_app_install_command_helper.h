// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;
class PrefService;

namespace content {
class WebContents;
}  // namespace content

namespace web_app {

enum class IconsDownloadedResult;
class IsolatedWebAppResponseReader;
class IsolatedWebAppResponseReaderFactory;
class UnusableSwbnFileError;
class WebAppDataRetriever;
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;

// This is a helper class that contains methods which are shared between both
// install and update commands.
class IsolatedWebAppInstallCommandHelper {
 public:
  static std::unique_ptr<IsolatedWebAppResponseReaderFactory>
  CreateDefaultResponseReaderFactory(const PrefService& prefs);

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

  void CheckTrustAndSignatures(
      const IsolatedWebAppLocation& location,
      Profile* profile,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void CreateStoragePartitionIfNotPresent(Profile& profile);

  void LoadInstallUrl(
      const IsolatedWebAppLocation& location,
      content::WebContents& web_contents,
      WebAppUrlLoader& url_loader,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  struct ManifestAndUrl {
    ManifestAndUrl(blink::mojom::ManifestPtr manifest, GURL url);
    ~ManifestAndUrl();

    ManifestAndUrl(const ManifestAndUrl&) = delete;
    ManifestAndUrl& operator=(const ManifestAndUrl&) = delete;

    ManifestAndUrl(ManifestAndUrl&&);
    ManifestAndUrl& operator=(ManifestAndUrl&&);

    blink::mojom::ManifestPtr manifest;
    GURL url;
  };

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents& web_contents,
      base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
          callback);

  base::expected<WebAppInstallInfo, std::string>
  ValidateManifestAndCreateInstallInfo(
      const absl::optional<base::Version>& expected_version,
      const ManifestAndUrl& manifest_and_url);

  void RetrieveIconsAndPopulateInstallInfo(
      WebAppInstallInfo install_info,
      content::WebContents& web_contents,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback);

 private:
  void CheckTrustAndSignaturesOfBundle(
      const base::FilePath& path,
      base::OnceCallback<void(base::expected<void, std::string>)> callback);

  void OnTrustAndSignaturesOfBundleChecked(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      base::expected<std::unique_ptr<IsolatedWebAppResponseReader>,
                     UnusableSwbnFileError> status);

  void OnLoadInstallUrl(
      base::OnceCallback<void(base::expected<void, std::string>)> callback,
      WebAppUrlLoaderResult result);

  void OnCheckInstallabilityAndRetrieveManifest(
      base::OnceCallback<void(base::expected<ManifestAndUrl, std::string>)>
          callback,
      blink::mojom::ManifestPtr opt_manifest,
      const GURL& manifest_url,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void OnRetrieveIcons(
      WebAppInstallInfo install_info,
      base::OnceCallback<void(base::expected<WebAppInstallInfo, std::string>)>
          callback,
      IconsDownloadedResult result,
      std::map<GURL, std::vector<SkBitmap>> icons_map,
      std::map<GURL, int /*http_status_code*/> unused_icons_http_results);

  IsolatedWebAppUrlInfo url_info_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;
  std::unique_ptr<IsolatedWebAppResponseReaderFactory> response_reader_factory_;

  base::WeakPtrFactory<IsolatedWebAppInstallCommandHelper> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALL_COMMAND_HELPER_H_
