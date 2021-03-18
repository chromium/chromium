// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

using content::BrowserThread;

namespace {

// Class that invokes a provided |callback| when destroyed, and supplies a means
// to keep the instance alive via posted tasks. The provided |callback| will
// always be invoked on the UI thread.
class Latch : public base::RefCountedThreadSafe<
                  Latch,
                  content::BrowserThread::DeleteOnUIThread> {
 public:
  explicit Latch(base::OnceClosure callback) : callback_(std::move(callback)) {}

  // Wraps a reference to |this| in a Closure and returns it. Running the
  // Closure does nothing. The Closure just serves to keep a reference alive
  // until |this| is ready to be destroyed; invoking the |callback|.
  base::RepeatingClosure NoOpClosure() {
    return base::BindRepeating(base::DoNothing::Repeatedly<Latch*>(),
                               base::RetainedRef(this));
  }

 private:
  friend class base::RefCountedThreadSafe<Latch>;
  friend class base::DeleteHelper<Latch>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  Latch(const Latch&) = delete;
  Latch& operator=(const Latch&) = delete;

  ~Latch() { std::move(callback_).Run(); }

  base::OnceClosure callback_;

};

}  // namespace

namespace web_app {

void RebuildAppAndLaunch(std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile =
      profile_manager->GetProfileByPath(shortcut_info->profile_path);
  if (!profile || !profile_manager->IsValidProfile(profile))
    return;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* extension = registry->GetExtensionById(
      shortcut_info->extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extension->is_platform_app())
    return;
  base::OnceCallback<void(base::Process)> launched_callback = base::DoNothing();
  base::OnceClosure terminated_callback = base::DoNothing();
  GetShortcutInfoForApp(
      extension, profile,
      base::BindOnce(
          &LaunchShim, LaunchShimUpdateBehavior::RECREATE_IF_INSTALLED,
          std::move(launched_callback), std::move(terminated_callback)));
}

bool MaybeRebuildShortcut(const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(app_mode::kAppShimError))
    return false;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RecordAppShimErrorAndBuildShortcutInfo,
                     command_line.GetSwitchValuePath(app_mode::kAppShimError)),
      base::BindOnce(&RebuildAppAndLaunch));
  return true;
}

// Mac-specific version of ShouldCreateShortcutFor() used during batch
// upgrades to ensure all shortcuts a user may still have are repaired when
// required by a Chrome upgrade.
bool ShouldUpgradeShortcutFor(Profile* profile,
                              const extensions::Extension* extension) {
  if (extension->location() ==
          extensions::mojom::ManifestLocation::kComponent ||
      !extensions::ui_util::CanDisplayInAppLauncher(extension, profile)) {
    return false;
  }

  return extension->is_app();
}

void UpdateShortcutsForAllApps(Profile* profile, base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return;

  scoped_refptr<Latch> latch = new Latch(std::move(callback));

  // Update all apps.
  std::unique_ptr<extensions::ExtensionSet> candidates =
      registry->GenerateInstalledExtensionsSet();
  for (auto& extension_refptr : *candidates) {
    const extensions::Extension* extension = extension_refptr.get();
    if (ShouldUpgradeShortcutFor(profile, extension)) {
      UpdateAllShortcuts(std::u16string(), profile, extension,
                         latch->NoOpClosure());
    }
  }
}

}  // namespace web_app

namespace chrome {

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const extensions::Extension* app,
    base::OnceCallback<void(bool)> close_callback) {
  // On Mac, the Applications folder is the only option, so don't bother asking
  // the user anything. Just create shortcuts.
  CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                  web_app::ShortcutLocations(), profile, app,
                  base::DoNothing());
  if (!close_callback.is_null())
    std::move(close_callback).Run(true);
}

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const std::string& app_id,
    base::OnceCallback<void(bool)> close_callback) {
  // On Mac, the Applications folder is the only option, so don't bother asking
  // the user anything. Just create shortcuts.
  CreateShortcutsForWebApp(web_app::SHORTCUT_CREATION_BY_USER,
                           web_app::ShortcutLocations(), profile, app_id,
                           base::DoNothing());
  if (!close_callback.is_null())
    std::move(close_callback).Run(true);
}

}  // namespace chrome
