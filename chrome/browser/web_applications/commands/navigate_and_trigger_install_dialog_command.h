// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class NoopLock;
class NoopLockDescription;
class WebAppDataRetriever;
class WebAppUrlLoader;
class WebAppUiManager;
enum class WebAppUrlLoaderResult;

enum class NavigateAndTriggerInstallDialogCommandResult {
  kFailure,
  kAlreadyInstalled,
  kDialogShown,
};
// The navigation will always succeed. The `result` indicates whether the
// command was able to trigger the install dialog.
using NavigateAndTriggerInstallDialogCommandCallback = base::OnceCallback<void(
    NavigateAndTriggerInstallDialogCommandResult result)>;

// This command navigates to the specified install url and waits for the web app
// manifest to load. If there is an installable web app that the user has not
// installed, the command will automatically trigger the install dialog.
class NavigateAndTriggerInstallDialogCommand
    : public WebAppCommandTemplate<NoopLock> {
 public:
  NavigateAndTriggerInstallDialogCommand(
      const GURL& install_url,
      const GURL& origin_url,
      bool is_renderer_initiated,
      NavigateAndTriggerInstallDialogCommandCallback callback,
      base::WeakPtr<WebAppUiManager> ui_manager,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      std::unique_ptr<WebAppDataRetriever> data_retriever,
      Profile* profile);
  ~NavigateAndTriggerInstallDialogCommand() override;

  // WebAppCommandTemplate<NoopLock>:
  const LockDescription& lock_description() const override;
  void StartWithLock(std::unique_ptr<NoopLock>) override;
  void OnShutdown() override;
  base::Value ToDebugValue() const override;

 private:
  bool IsWebContentsDestroyed();

  void OnUrlLoaded(WebAppUrlLoaderResult result);
  void OnInstallabilityChecked(blink::mojom::ManifestPtr opt_manifest,
                               const GURL& manifest_url,
                               bool valid_manifest_for_web_app,
                               webapps::InstallableStatusCode error_code);
  void OnAppLockGranted(std::unique_ptr<AppLock> app_lock);
  void Abort(NavigateAndTriggerInstallDialogCommandResult result);

  std::unique_ptr<NoopLockDescription> noop_lock_description_;
  std::unique_ptr<AppLockDescription> app_lock_description_;

  std::unique_ptr<AppLock> app_lock_;
  std::unique_ptr<NoopLock> noop_lock_;

  const GURL install_url_;
  const GURL origin_url_;
  const bool is_renderer_initiated_;
  NavigateAndTriggerInstallDialogCommandCallback callback_;

  base::WeakPtr<WebAppUiManager> ui_manager_;
  const std::unique_ptr<WebAppUrlLoader> url_loader_;
  const std::unique_ptr<WebAppDataRetriever> data_retriever_;
  raw_ptr<Profile> profile_;

  AppId app_id_;
  raw_ptr<content::WebContents> web_contents_;
  base::Value::List error_log_;

  base::WeakPtrFactory<NavigateAndTriggerInstallDialogCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_NAVIGATE_AND_TRIGGER_INSTALL_DIALOG_COMMAND_H_
