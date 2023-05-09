// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_uninstall_and_replace_job.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace content {
class WebContents;
}

namespace web_app {

class AppLock;
class AppLockDescription;
class LockDescription;
class WebAppDataRetriever;

// This is used during externally managed app install flow to install a
// placeholder app instead of the target app when the app's install_url fails to
// load.
class InstallPlaceholderCommand : public WebAppCommandTemplate<AppLock> {
 public:
  using InstallAndReplaceCallback =
      base::OnceCallback<void(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace)>;
  InstallPlaceholderCommand(
      Profile* profile,
      const ExternalInstallOptions& install_options,
      InstallAndReplaceCallback callback,
      base::WeakPtr<content::WebContents> web_contents,
      std::unique_ptr<WebAppDataRetriever> data_retriever);

  ~InstallPlaceholderCommand() override;

  // WebAppCommandTemplate<AppLock>:
  void StartWithLock(std::unique_ptr<AppLock> lock) override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnShutdown() override;

 private:
  void Abort(webapps::InstallResultCode code);
  void FetchCustomIcon(const GURL& url, int retries_left);

  void OnCustomIconFetched(const GURL& image_url,
                           int retries_left,
                           IconsDownloadedResult result,
                           IconsMap icons_map,
                           DownloadedIconsHttpResults icons_http_results);

  void FinalizeInstall(
      absl::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
          bitmaps);

  void OnInstallFinalized(const AppId& app_id,
                          webapps::InstallResultCode code,
                          OsHooksErrors os_hooks_errors);
  void OnUninstallAndReplaced(const AppId& app_id,
                              webapps::InstallResultCode code,
                              bool did_uninstall_and_replace);

  const raw_ptr<Profile> profile_;
  const AppId app_id_;
  std::unique_ptr<AppLockDescription> lock_description_;
  std::unique_ptr<AppLock> lock_;

  const ExternalInstallOptions install_options_;
  InstallAndReplaceCallback callback_;
  base::WeakPtr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppDataRetriever> data_retriever_;

  absl::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;

  base::Value::Dict debug_value_;

  base::WeakPtrFactory<InstallPlaceholderCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_
