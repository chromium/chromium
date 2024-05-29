// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

class WithAppResources;

struct ShortcutInfo;
struct ShortcutLocations;

// Uninstalls the web apps or extensions in |from_apps_or_extensions| and
// migrates an |to_app|'s OS attributes (e.g pin position, app list
// folder/position, shortcuts and other OS integrations) to the first |from_app|
// found.
// The app lock only needs to lock the app with the `to_app` id. The
// `from_apps_or_extensions` are uninstalled through separately scheduled
// commands.
class WebAppUninstallAndReplaceJob {
 public:
  WebAppUninstallAndReplaceJob(
      Profile* profile,
      base::Value::Dict& debug_value,
      WithAppResources& to_app_lock,
      const std::vector<webapps::AppId>& from_apps_or_extensions,
      const webapps::AppId& to_app,
      base::OnceCallback<void(bool uninstall_triggered)> on_complete);
  ~WebAppUninstallAndReplaceJob();

  // Note: This can synchronously call `on_complete`.
  void Start();

 private:
  void MigrateUiAndUninstallApp(const webapps::AppId& from_app,
                                base::OnceClosure on_complete);
  void OnMigrateLauncherState(const webapps::AppId& from_app,
                              base::OnceClosure on_complete);
  void OnShortcutInfoReceivedSearchShortcutLocations(
      const webapps::AppId& from_app,
      base::OnceClosure on_complete,
      std::unique_ptr<ShortcutInfo> shortcut_info);

  void OnShortcutLocationGathered(const webapps::AppId& from_app,
                                  base::OnceClosure on_complete,
                                  ShortcutLocations from_app_locations);

  void SynchronizeOSIntegrationForReplacementApp(
      base::OnceClosure on_complete,
      bool from_app_run_on_os_login,
      ShortcutLocations from_app_locations);

  const raw_ref<Profile> profile_;
  const raw_ref<base::Value::Dict> debug_value_;
  // `this` must exist within the scope of a WebAppCommand's WithAppResources.
  const raw_ref<WithAppResources> to_app_lock_;
  std::vector<webapps::AppId> from_apps_or_extensions_;
  const webapps::AppId to_app_;
  base::OnceCallback<void(bool uninstall_triggered)> on_complete_;

  base::WeakPtrFactory<WebAppUninstallAndReplaceJob> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_JOBS_UNINSTALL_WEB_APP_UNINSTALL_AND_REPLACE_JOB_H_
