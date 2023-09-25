// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/jobs/install_placeholder_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace web_app {

class SharedWebContentsWithAppLock;
class SharedWebContentsWithAppLockDescription;
class LockDescription;
class WebAppDataRetriever;
class WebAppUninstallAndReplaceJob;

// This is used during externally managed app install flow to install a
// placeholder app instead of the target app when the app's install_url fails to
// load.
class InstallPlaceholderCommand
    : public WebAppCommandTemplate<SharedWebContentsWithAppLock> {
 public:
  using InstallAndReplaceCallback =
      base::OnceCallback<void(ExternallyManagedAppManager::InstallResult)>;
  InstallPlaceholderCommand(Profile* profile,
                            const ExternalInstallOptions& install_options,
                            InstallAndReplaceCallback callback);

  ~InstallPlaceholderCommand() override;

  // WebAppCommandTemplate<SharedWebContentsWithAppLock>:
  void StartWithLock(
      std::unique_ptr<SharedWebContentsWithAppLock> lock) override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;
  void OnShutdown() override;

  // TODO(b/299879507): Remove explicit data retriever setting for tests.
  void SetDataRetrieverForTesting(
      std::unique_ptr<WebAppDataRetriever> data_retriever);

 private:
  void Abort(webapps::InstallResultCode code);
  void OnPlaceholderInstalled(webapps::InstallResultCode code,
                              webapps::AppId app_id);
  void OnUninstallAndReplaced(webapps::InstallResultCode code,
                              bool did_uninstall_and_replace);

  const raw_ptr<Profile> profile_;
  const webapps::AppId app_id_;
  std::unique_ptr<SharedWebContentsWithAppLockDescription> lock_description_;
  std::unique_ptr<SharedWebContentsWithAppLock> lock_;

  const ExternalInstallOptions install_options_;
  InstallAndReplaceCallback callback_;

  absl::optional<InstallPlaceholderJob> install_placeholder_job_;
  absl::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;

  std::unique_ptr<WebAppDataRetriever> data_retriever_for_testing_;

  base::WeakPtrFactory<InstallPlaceholderCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_PLACEHOLDER_COMMAND_H_
