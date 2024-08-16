// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/one_shot_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#import "chrome/common/mac/app_mode_common.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
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

  Latch(const Latch&) = delete;
  Latch& operator=(const Latch&) = delete;

  // Wraps a reference to |this| in a Closure and returns it. Running the
  // Closure does nothing. The Closure just serves to keep a reference alive
  // until |this| is ready to be destroyed; invoking the |callback|.
  base::RepeatingClosure NoOpClosure() {
    return base::BindRepeating([](Latch*) {}, base::RetainedRef(this));
  }

 private:
  friend class base::RefCountedThreadSafe<Latch>;
  friend class base::DeleteHelper<Latch>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  ~Latch() { std::move(callback_).Run(); }

  base::OnceClosure callback_;

};

}  // namespace

namespace web_app {

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

  // Wait for extensions to be ready before continuing.
  auto* extension_system = extensions::ExtensionSystem::Get(profile);
  if (!extension_system)
    return;
  if (!extension_system->is_ready()) {
    extension_system->ready().Post(
        FROM_HERE, base::BindOnce(&UpdateShortcutsForAllApps, profile,
                                  std::move(callback)));
    return;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  if (!registry)
    return;

  // Note: This can be replaced with a `BarrierCallback`, and the callback is
  // now guaranteed to be called on the calling thread.
  scoped_refptr<Latch> latch = new Latch(std::move(callback));

  // Update all apps.
  extensions::ExtensionSet candidates =
      registry->GenerateInstalledExtensionsSet();
  for (auto& extension_refptr : candidates) {
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
                  base::BindOnce(std::move(close_callback)));
}

void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow /*parent_window*/,
    Profile* profile,
    const std::string& app_id,
    base::OnceCallback<void(bool)> close_callback) {
  // On Mac, the Applications folder is the only option, so don't bother asking
  // the user anything. Just create shortcuts via OS integration.
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  provider->scheduler().SynchronizeOsIntegration(
      app_id, base::BindOnce(std::move(close_callback), true),
      web_app::ConvertShortcutLocationsToSynchronizeOptions(
          web_app::ShortcutLocations(), web_app::SHORTCUT_CREATION_BY_USER),
      /*upgrade_to_fully_installed_if_installed=*/true);
}

}  // namespace chrome
