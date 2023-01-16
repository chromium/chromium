// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"

class GURL;

namespace web_app {

class LockDescription;
class SharedWebContentsLock;
class SharedWebContentsLockDescription;
class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;

// Installs a web app using a raw manifest JSON string, which is interpreted as
// if it was loaded from the renderer for a given URL. This does not attempt to
// verify all the installability criteria of the manifest: the manifest is
// treated as valid if it parses successfully and contains a start URL.
//
// The web app can be simultaneously installed from multiple sources. If the web
// app already exists, the manifest contents will be ignored.
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
  // `callback`: Called when installation completes.
  InstallFromManifestCommand(webapps::WebappInstallSource install_source,
                             GURL document_url,
                             GURL manifest_url,
                             std::string manifest_contents,
                             AppId expected_id,
                             OnceInstallCallback callback);

  ~InstallFromManifestCommand() override;

  // WebAppCommandTemplate:
  LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<SharedWebContentsLock> lock) override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  void OnManifestParsed(blink::mojom::ManifestPtr manifest);
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
  OnceInstallCallback install_callback_;

  // SharedWebContentsLock is held while parsing the manifest.
  std::unique_ptr<SharedWebContentsLock> web_contents_lock_;
  std::unique_ptr<SharedWebContentsLockDescription>
      web_contents_lock_description_;

  // SharedWebContentsWithAppLock is held while installing the app.
  std::unique_ptr<SharedWebContentsWithAppLock> app_lock_;
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      app_lock_description_;

  bool manifest_parsed_ = false;
  std::unique_ptr<WebAppInstallInfo> web_app_info_;

  mojo::Remote<blink::mojom::ManifestManager> manifest_manager_;

  base::WeakPtrFactory<InstallFromManifestCommand> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_FROM_MANIFEST_COMMAND_H_
