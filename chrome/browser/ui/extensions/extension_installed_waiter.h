// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WAITER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WAITER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class Browser;

// ExtensionInstalledWaiter is used to wait for a given extension to be
// installed in a given browser's profile.
class ExtensionInstalledWaiter : public extensions::ExtensionRegistryObserver,
                                 public BrowserListObserver {
 public:
  // Wait until both:
  // 1. |extension| is installed into |browser|
  // 2. All EXTENSION_LOADED observers have been notified of (1)
  // and then invoke |done_callback|.
  // If either |browser| is destroyed or |extension| is uninstalled from it
  // before that happens, |done_callback| is not run.
  static void WaitForInstall(
      scoped_refptr<const extensions::Extension> extension,
      Browser* browser,
      base::OnceClosure done_callback);

  // Sets a callback for testing purposes to be invoked whenever an
  // ExtensionInstalledWaiter gives up on waiting for any reason. You should not
  // need this in production code!
  static void SetGivingUpCallbackForTesting(base::RepeatingClosure callback);

 private:
  // This class manages its own lifetime.
  ExtensionInstalledWaiter(scoped_refptr<const extensions::Extension> extension,
                           Browser* browser,
                           base::OnceClosure done_callback);
  ~ExtensionInstalledWaiter() override;

  ExtensionInstalledWaiter(const ExtensionInstalledWaiter& other) = delete;
  ExtensionInstalledWaiter& operator=(const ExtensionInstalledWaiter& other) =
      delete;

  // Check if the extension is installed. If so, run |done_callback_| and
  // self-destruct.
  void RunCallbackIfExtensionInstalled();

  // Returns whether condition (1) as described above WaitForInstall
  // are true. Condition (2) is guaranteed by logic in OnExtensionLoaded.
  bool IsExtensionInstalled() const;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  const scoped_refptr<const extensions::Extension> extension_;
  const raw_ptr<const Browser> browser_;
  base::OnceClosure done_callback_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<ExtensionInstalledWaiter> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_WAITER_H_
