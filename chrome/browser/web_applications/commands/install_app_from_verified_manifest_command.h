// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_FROM_VERIFIED_MANIFEST_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_FROM_VERIFIED_MANIFEST_COMMAND_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

namespace webapps {
class WebAppUrlLoader;
enum class WebAppUrlLoaderResult;
}  // namespace webapps

namespace web_app {

class SharedWebContentsWithAppLock;

// Installs a web app using a raw manifest JSON string, bypassing the usual
// network fetch that ensures the provided manifest is representative of what
// is rel="mainfest" linked to by `document_url`.
//
// A limited set of checks must pass for the app to to successfully install:
// - The manifest must be valid JSON
// - The manifest must have a valid start URL and name/short_name
// - The manifest must have a valid icon from an allowlisted host
//
// Installation will fail if any of these criteria are not met, or if icons fail
// to download (that is, placeholder icons will never be generated).
//
// The web app can be simultaneously installed from multiple sources. If the
// web app already exists, the manifest contents will be ignored.
class InstallAppFromVerifiedManifestCommand
    : public WebAppCommand<SharedWebContentsLock,
                           const webapps::AppId&,
                           webapps::InstallResultCode> {
 public:
  // Begins installation of a web app from a raw manifest string.
  //
  // `install_source`: Source of this web app installation
  // `document_url`: URL of the HTML document where the manifest was found. Used
  // for manifest parsing.
  // `verified_manifest_url`: URL that the manifest content was retrieved from.
  // Used for manifest parsing.
  // `verified_manifest_contents`: JSON string of a web app manifest to install.
  // `expected_id`: Expected hashed App ID for the installed app. If the ID does
  // not match, installation will abort with an error.
  // `is_diy_app`: When true, treat this install as "DIY", meaning that the
  // manifest content may be incomplete or supplemented from alternative
  // sources.
  // `install_params`: Additional optional params applied to customize the
  // installed app.
  // `callback`: Called when installation completes.
  InstallAppFromVerifiedManifestCommand(
      webapps::WebappInstallSource install_source,
      GURL document_url,
      GURL verified_manifest_url,
      std::string verified_manifest_contents,
      webapps::AppId expected_id,
      bool is_diy_app,
      std::optional<WebAppInstallParams> install_params,
      OnceInstallCallback callback);

  ~InstallAppFromVerifiedManifestCommand() override;

 protected:
  // WebAppCommand:
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;

 private:
  void OnAboutBlankLoaded(webapps::WebAppUrlLoaderResult result);
  void OnManifestParsed(blink::mojom::ManifestPtr manifest);
  void OnIconsRetrieved(IconsDownloadedResult result,
                        IconsMap icons_map,
                        DownloadedIconsHttpResults icons_http_results);
  void OnAppLockAcquired();
  void OnInstallFinalized(const webapps::AppId& app_id,
                          webapps::InstallResultCode code);

  void Abort(CommandResult result, webapps::InstallResultCode code);

  webapps::WebappInstallSource install_source_;
  GURL document_url_;
  GURL verified_manifest_url_;
  std::string verified_manifest_contents_;
  webapps::AppId expected_id_;
  bool is_diy_app_;
  std::optional<WebAppInstallParams> install_params_;

  // SharedWebContentsLock is held while parsing the manifest.
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;

  // SharedWebContentsWithAppLock is held while installing the app.
  std::unique_ptr<SharedWebContentsWithAppLock> app_lock_;

  std::unique_ptr<webapps::WebAppUrlLoader> url_loader_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;

  base::WeakPtrFactory<InstallAppFromVerifiedManifestCommand> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_APP_FROM_VERIFIED_MANIFEST_COMMAND_H_
