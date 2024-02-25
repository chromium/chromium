// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
enum class InstallResultCode;
enum class UninstallResultCode;
}  // namespace webapps

namespace web_app {

class WebAppProvider;
class WebAppDataRetriever;

// Class to install WebApp from a WebContents. A queue of such tasks is owned by
// ExternallyManagedAppManager. Can only be called from the UI thread.
class ExternallyManagedAppInstallTask {
 public:
  using ResultCallback = base::OnceCallback<void(
      ExternallyManagedAppManager::InstallResult result)>;
  using DataRetrieverFactory =
      base::RepeatingCallback<std::unique_ptr<WebAppDataRetriever>()>;

  // Constructs a task that will install a Web App for |profile|.
  // |install_options| will be used to decide some of the properties of the
  // installed app e.g. open in a tab vs. window, installed by policy, etc.
  explicit ExternallyManagedAppInstallTask(
      WebAppProvider& provider,
      ExternalInstallOptions install_options);

  ExternallyManagedAppInstallTask(const ExternallyManagedAppInstallTask&) =
      delete;
  ExternallyManagedAppInstallTask& operator=(
      const ExternallyManagedAppInstallTask&) = delete;

  virtual ~ExternallyManagedAppInstallTask();

  virtual void Install(
      std::optional<webapps::AppId> installed_placeholder_app_id,
      ResultCallback result_callback);

  const ExternalInstallOptions& install_options() { return install_options_; }

 private:
  const raw_ref<WebAppProvider> provider_;
  const ExternalInstallOptions install_options_;

  base::WeakPtrFactory<ExternallyManagedAppInstallTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTERNALLY_MANAGED_APP_INSTALL_TASK_H_
