// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WATCHER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WATCHER_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class Extension;
}

class ExtensionInstalledWatcher : public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionInstalledWatcher(Profile* profile);
  ~ExtensionInstalledWatcher() override;

  ExtensionInstalledWatcher(const ExtensionInstalledWatcher&) = delete;
  ExtensionInstalledWatcher& operator=(const ExtensionInstalledWatcher&) =
      delete;

  void WaitForInstall(const extensions::ExtensionId& extension_id,
                      base::OnceCallback<void(bool)> done_callback);

 private:
  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  void RunCallback(const extensions::ExtensionId& extension_id, bool installed);

  raw_ptr<Profile> profile_;

  // Map to track multiple pending installs
  // The bool indicates if the extension was installed or not
  std::map<extensions::ExtensionId, base::OnceCallback<void(bool)>>
      pending_installs_;

  base::WeakPtrFactory<ExtensionInstalledWatcher> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WATCHER_H_
