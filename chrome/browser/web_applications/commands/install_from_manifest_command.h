// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "url/gurl.h"

namespace web_app {

class LockDescription;
class SharedWebContentsLock;
class SharedWebContentsLockDescription;
class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;

// Installs a web app using a raw manifest JSON string, which is interpreted as
// if it was loaded from the renderer for a given URL. This does not attempt to
// verify all the normal installability criteria of the manifest, instead it
// just checks limited criteria needed to successfully install the app:
// - The manifest must be valid JSON
// - The manifest must have a valid start URL and name/short_name
// - The manifest must have a valid icon from an allowlisted host (see
// `host_allowlist` parameter).
//
// Installation will fail if any of these criteria are not met, or if icons fail
// to download (that is, placeholder icons will never be generated).
//
// The web app can be simultaneously installed from multiple sources. If the
// web app already exists, the manifest contents will be ignored.
class InstallFromManifestCommand
    : public WebAppCommandTemplate<SharedWebContentsLock> {
 public:
  // Begins installation of a web app from a raw manifest string.
  //
  // `install_source`: Source of this web app installation
  // `document_url`: URL of the HTML document where the manifest was found. Used
  // for manifest parsing.
  // `manifest_url`: URL that the manifest content was retrieved from. Used for
  // manifest parsing.
  // `manifest_contents`: JSON string of a web app manifest to install.
  // `expected_id`: Expected hashed App ID for the installed app. If the ID does
  // not match, installation will abort with an error.
  // `host_allowlist`: Allowlist of hosts which icon data can be downloaded
  // from. Icon URLs whose host does not exactly match a host from this set are
  // ignored.
  // `callback`: Called when installation completes.
  InstallFromManifestCommand(webapps::WebappInstallSource install_source,
                             GURL document_url,
                             GURL manifest_url,
                             std::string manifest_contents,
                             AppId expected_id,
                             base::flat_set<std::string> host_allowlist,
                             OnceInstallCallback callback);

  ~InstallFromManifestCommand() override;

  // WebAppCommandTemplate<SharedWebContentsLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  void OnManifestParsed(blink::mojom::ManifestPtr manifest);
  void OnIconsRetrieved(IconsDownloadedResult result,
                        IconsMap icons_map,
                        DownloadedIconsHttpResults icons_http_results);
  void OnAppLockAcquired(
      std::unique_ptr<SharedWebContentsWithAppLock> app_lock);
  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);

  void Abort(CommandResult result, webapps::InstallResultCode code);

  webapps::WebappInstallSource install_source_;
  GURL document_url_;
  GURL manifest_url_;
  std::string manifest_contents_;
  AppId expected_id_;
  base::flat_set<std::string> host_allowlist_;
  OnceInstallCallback install_callback_;

  // SharedWebContentsLock is held while parsing the manifest.
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<SharedWebContentsLockDescription>
      web_contents_lock_description_;

  // SharedWebContentsWithAppLock is held while installing the app.
  std::unique_ptr<SharedWebContentsWithAppLock> app_lock_;
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      app_lock_description_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  base::Value::Dict debug_value_;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;

  base::WeakPtrFactory<InstallFromManifestCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_
