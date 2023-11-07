// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_UTILS_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"

class GURL;

namespace blink {
namespace mojom {
class Manifest;
}  // namespace mojom
}  // namespace blink

namespace content {
class WebContents;
}

namespace webapps {
enum class WebappInstallSource;
enum class WebappUninstallSource;
}  // namespace webapps

namespace web_app {

class WebApp;
class WebAppRegistrar;
struct WebAppInstallParams;

enum class ForInstallableSite {
  kYes,
  kNo,
  kUnknown,
};

// Converts from the manifest type to the Chrome type.
apps::FileHandlers CreateFileHandlersFromManifest(
    const std::vector<blink::mojom::ManifestFileHandlerPtr>&
        manifest_file_handlers,
    const GURL& app_scope);

// Update the given WebAppInstallInfo with information from the manifest.
// Will sanitise the manifest fields to be suitable for installation to prevent
// sites from using arbitrarily large amounts of disk space.
void UpdateWebAppInfoFromManifest(const blink::mojom::Manifest& manifest,
                                  const GURL& manifest_url,
                                  WebAppInstallInfo* web_app_info);

// Same as above, but returns a fresh WebAppInstallInfo.
WebAppInstallInfo CreateWebAppInfoFromManifest(
    const blink::mojom::Manifest& manifest,
    const GURL& manifest_url);

// Form a list of icons to download: Remove icons with invalid urls.
base::flat_set<GURL> GetValidIconUrlsToDownload(
    const WebAppInstallInfo& web_app_info);

// Populate non-product icons in WebAppInstallInfo using the IconsMap. This
// currently covers shortcut item icons and file handler icons. It ignores
// icons that might have already existed in `web_app_info`.
void PopulateOtherIcons(WebAppInstallInfo* web_app_info,
                        const IconsMap& icons_map);

// Populates main product icons into `web_app_info`. This method filters icons
// from `icons_map` to only square icons and ensures that the necessary-sized
// icons are available by resizing larger icons down to smaller sizes. When
// `icons_map` is null or missing icons, it will generate icons for sizes where
// resizing is not possible. Icons which were already populated in
// `web_app_info` may be retained, and even used to generate missing icons.
void PopulateProductIcons(WebAppInstallInfo* web_app_info,
                          const IconsMap* icons_map);

// Records downloaded icons result and http code and code class.
void RecordDownloadedIconsResultAndHttpStatusCodes(
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results);

// Records the class of http status code (2XX, 3XX, 4XX, 5XX) for each processed
// icon url.
void RecordDownloadedIconsHttpResultsCodeClass(
    base::StringPiece histogram_name,
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results);

// Records http status code for each processed icon url.
void RecordDownloadedIconHttpStatusCodes(
    base::StringPiece histogram_name,
    const DownloadedIconsHttpResults& icons_http_results);

WebAppManagement::Type ConvertExternalInstallSourceToSource(
    ExternalInstallSource external_install_source);

webapps::WebappInstallSource ConvertExternalInstallSourceToInstallSource(
    ExternalInstallSource external_install_source);

webapps::WebappUninstallSource ConvertExternalInstallSourceToUninstallSource(
    ExternalInstallSource external_install_source);

// Infer the web app source from the installation surface.
WebAppManagement::Type ConvertInstallSurfaceToWebAppSource(
    webapps::WebappInstallSource install_surface);

void CreateWebAppInstallTabHelpers(content::WebContents* web_contents);

// The function should be called before removing a source from the WebApp.
void MaybeRegisterOsUninstall(const WebApp* web_app,
                              WebAppManagement::Type source_uninstalling,
                              OsIntegrationManager& os_integration_manager,
                              InstallOsHooksCallback callback);

// The function should be called before adding source to the WebApp.
void MaybeUnregisterOsUninstall(const WebApp* web_app,
                                WebAppManagement::Type source_installing,
                                OsIntegrationManager& os_integration_manager);

// Updates |web_app| using |web_app_info|
void SetWebAppManifestFields(const WebAppInstallInfo& web_app_info,
                             WebApp& web_app,
                             bool skip_icons_on_download_failure = false);

// Updates product icon fields of |web_app| using |web_app_info|.
void SetWebAppProductIconFields(const WebAppInstallInfo& web_app_info,
                                WebApp& web_app);

// Possibly updates |options| to disable OS-integrations based on the
// configuration of the given app.
void MaybeDisableOsIntegration(const WebAppRegistrar* app_registrar,
                               const webapps::AppId& app_id,
                               InstallOsHooksOptions* options);

// Update |web_app_info| using |install_params|.
void ApplyParamsToWebAppInstallInfo(const WebAppInstallParams& install_params,
                                    WebAppInstallInfo& web_app_info);

// Update |options| using |install_params|.
void ApplyParamsToFinalizeOptions(
    const WebAppInstallParams& install_params,
    WebAppInstallFinalizer::FinalizeOptions& options);
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_INSTALL_UTILS_H_
