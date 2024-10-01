// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_IWA_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_IWA_INTERNALS_HANDLER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace web_app {

class IwaSourceDevModeWithFileOp;
class ScopedTempWebBundleFile;
class WebAppInternalsIwaInstallationBrowserTest;
struct InstallIsolatedWebAppCommandSuccess;

// Handles API requests from chrome://web-app-internals related to IWAs.
// This class is supposed to be used by WebAppInternalsHandler.
class IwaInternalsHandler {
 public:
  using Handler = ::mojom::WebAppInternalsHandler;

  IwaInternalsHandler(content::WebUI& web_ui, Profile& profile);
  ~IwaInternalsHandler();

  IwaInternalsHandler(const IwaInternalsHandler&) = delete;
  IwaInternalsHandler& operator=(const IwaInternalsHandler&) = delete;

  void InstallIsolatedWebAppFromDevProxy(
      const GURL& url,
      Handler::InstallIsolatedWebAppFromDevProxyCallback callback);

  void ParseUpdateManifestFromUrl(
      const GURL& update_manifest_url,
      Handler::ParseUpdateManifestFromUrlCallback callback);

  void InstallIsolatedWebAppFromBundleUrl(
      ::mojom::InstallFromBundleUrlParamsPtr params,
      Handler::InstallIsolatedWebAppFromBundleUrlCallback callback);

  void SelectFileAndInstallIsolatedWebAppFromDevBundle(
      Handler::SelectFileAndInstallIsolatedWebAppFromDevBundleCallback
          callback);

  void SelectFileAndUpdateIsolatedWebAppFromDevBundle(
      const webapps::AppId& app_id,
      Handler::SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback);

  void SearchForIsolatedWebAppUpdates(
      Handler::SearchForIsolatedWebAppUpdatesCallback callback);

  void GetIsolatedWebAppDevModeAppInfo(
      Handler::GetIsolatedWebAppDevModeAppInfoCallback callback);

  void UpdateDevProxyIsolatedWebApp(
      const webapps::AppId& app_id,
      Handler::UpdateDevProxyIsolatedWebAppCallback callback);

  void RotateKey(const std::string& web_bundle_id,
                 const std::optional<std::vector<uint8_t>>& public_key);

  void UpdateManifestInstalledIsolatedWebApp(
      const webapps::AppId& app_id,
      Handler::UpdateManifestInstalledIsolatedWebAppCallback callback);

 private:
  class IsolatedWebAppDevBundleSelectListener;
  class IwaManifestInstallUpdateHandler;
  friend class web_app::WebAppInternalsIwaInstallationBrowserTest;

  Profile* profile() { return &profile_.get(); }

  void DownloadWebBundleToFile(
      const GURL& web_bundle_url,
      const GURL& update_manifest_url,
      Handler::InstallIsolatedWebAppFromBundleUrlCallback callback,
      web_app::ScopedTempWebBundleFile file);

  void OnWebBundleDownloaded(
      const GURL& update_manifest_url,
      Handler::InstallIsolatedWebAppFromBundleUrlCallback callback,
      web_app::ScopedTempWebBundleFile bundle,
      int32_t result);

  void OnIsolatedWebAppDevModeBundleSelected(
      Handler::SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback,
      std::optional<base::FilePath> path);

  void OnIsolatedWebAppDevModeBundleSelectedForUpdate(
      const webapps::AppId& app_id,
      Handler::SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback,
      std::optional<base::FilePath> path);

  void OnInstallIsolatedWebAppInDevMode(
      base::OnceCallback<void(::mojom::InstallIsolatedWebAppResultPtr)>
          callback,
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string> result);

  void OnInstalledIsolatedWebAppInDevModeFromWebBundle(
      const GURL& update_manifest_url,
      base::OnceCallback<void(::mojom::InstallIsolatedWebAppResultPtr)>
          callback,
      base::expected<InstallIsolatedWebAppCommandSuccess, std::string> result);

  // Discovers and applies an update for a dev mode Isolated Web App identified
  // by its app id. If `location` is set, then the update will be read from the
  // provided location, otherwise the existing location will be used.
  void ApplyDevModeUpdate(
      const webapps::AppId& app_id,
      base::optional_ref<const web_app::IwaSourceDevModeWithFileOp> location,
      base::OnceCallback<void(const std::string&)> callback);

  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<Profile> profile_;

  // Runs updates for manifest-installed dev-mode apps.
  // Will be nullptr if WebAppProvider is not available for the current
  // `profile_`.
  std::unique_ptr<IwaManifestInstallUpdateHandler> update_handler_;

  base::WeakPtrFactory<IwaInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_IWA_INTERNALS_HANDLER_H_
