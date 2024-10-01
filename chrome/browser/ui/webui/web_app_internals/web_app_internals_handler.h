// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/web_app_internals/iwa_internals_handler.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebUI;
}  // namespace content

// Handles API requests from chrome://web-app-internals page by implementing
// mojom::WebAppInternalsHandler.
class WebAppInternalsHandler : public mojom::WebAppInternalsHandler {
 public:
  static void BuildDebugInfo(
      Profile* profile,
      base::OnceCallback<void(base::Value root)> callback);

  WebAppInternalsHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<mojom::WebAppInternalsHandler> receiver);

  WebAppInternalsHandler(const WebAppInternalsHandler&) = delete;
  WebAppInternalsHandler& operator=(const WebAppInternalsHandler&) = delete;

  ~WebAppInternalsHandler() override;

  // mojom::WebAppInternalsHandler:
  void GetDebugInfoAsJsonString(
      GetDebugInfoAsJsonStringCallback callback) override;
  void InstallIsolatedWebAppFromDevProxy(
      const GURL& url,
      InstallIsolatedWebAppFromDevProxyCallback callback) override;
  void ParseUpdateManifestFromUrl(
      const GURL& update_manifest_url,
      ParseUpdateManifestFromUrlCallback callback) override;
  void InstallIsolatedWebAppFromBundleUrl(
      mojom::InstallFromBundleUrlParamsPtr params,
      InstallIsolatedWebAppFromBundleUrlCallback callback) override;
  void SelectFileAndInstallIsolatedWebAppFromDevBundle(
      SelectFileAndInstallIsolatedWebAppFromDevBundleCallback callback)
      override;
  void SelectFileAndUpdateIsolatedWebAppFromDevBundle(
      const webapps::AppId& app_id,
      SelectFileAndUpdateIsolatedWebAppFromDevBundleCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void ClearExperimentalWebAppIsolationData(
      ClearExperimentalWebAppIsolationDataCallback callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  void SearchForIsolatedWebAppUpdates(
      SearchForIsolatedWebAppUpdatesCallback callback) override;
  void GetIsolatedWebAppDevModeAppInfo(
      GetIsolatedWebAppDevModeAppInfoCallback callback) override;
  void UpdateDevProxyIsolatedWebApp(
      const webapps::AppId& app_id,
      UpdateDevProxyIsolatedWebAppCallback callback) override;
  void RotateKey(
      const std::string& web_bundle_id,
      const std::optional<std::vector<uint8_t>>& public_key) override;
  void UpdateManifestInstalledIsolatedWebApp(
      const webapps::AppId& app_id,
      UpdateManifestInstalledIsolatedWebAppCallback callback) override;

 private:
  const raw_ref<content::WebUI> web_ui_;
  const raw_ref<Profile> profile_;
  mojo::Receiver<mojom::WebAppInternalsHandler> receiver_;
  web_app::IwaInternalsHandler iwa_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_APP_INTERNALS_WEB_APP_INTERNALS_HANDLER_H_
