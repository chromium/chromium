// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"

class Profile;

namespace web_app {

class AppLock;

struct ShortcutInfo;
struct ShortcutLocations;

// Uninstalls the web apps or extensions in |from_apps_or_extensions| and
// migrates an |to_app|'s OS attributes (e.g pin position, app list
// folder/position, shortcuts and other OS integrations) to the first |from_app|
// found.
class WebAppUninstallAndReplaceJob {
 public:
  WebAppUninstallAndReplaceJob(
      Profile* profile,
      AppLock& to_app_lock,
      const std::vector<AppId>& from_apps_or_extensions,
      const AppId& to_app,
      base::OnceCallback<void(bool uninstall_triggered)> on_complete);
  ~WebAppUninstallAndReplaceJob();
  // Note: This can synchronously call `on_complete`.
  void Start();

 private:
  void MigrateUiAndUninstallApp(const AppId& from_app,
                                base::OnceClosure on_complete);
  void OnMigrateLauncherState(const AppId& from_app,
                              base::OnceClosure on_complete);
  void OnShortcutInfoReceivedSearchShortcutLocations(
      const AppId& from_app,
      base::OnceClosure on_complete,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  void OnShortcutLocationGathered(const AppId& from_app,
                                  base::OnceClosure on_complete,
                                  ShortcutLocations locations);

  void InstallOsHooksForReplacementApp(base::OnceClosure on_complete,
                                       ShortcutLocations locations);

  void OnInstallOsHooksCompleted(base::OnceClosure on_complete, OsHooksErrors);

  raw_ptr<Profile> profile_ = nullptr;
  // `this` must exist within the scope of a WebAppCommand's AppLock.
  raw_ref<AppLock> to_app_lock_;
  std::vector<AppId> from_apps_or_extensions_;
  const AppId to_app_;
  base::OnceCallback<void(bool uninstall_triggered)> on_complete_;

  base::WeakPtrFactory<WebAppUninstallAndReplaceJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_
