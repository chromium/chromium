// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_REMOVAL_WATCHER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_REMOVAL_WATCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

// ExtensionRemovalWatcher watches a browser and an extension for either:
// 1) The browser being closed, or
// 2) The extension being uninstalled from the browser's profile
// and in either case, invokes the provided callback.
class ExtensionRemovalWatcher : public BrowserListObserver,
                                public extensions::ExtensionRegistryObserver {
 public:
  ExtensionRemovalWatcher(Browser* browser,
                          scoped_refptr<const extensions::Extension> extension,
                          base::OnceClosure callback);
  ~ExtensionRemovalWatcher() override;

 private:
  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // BrowserListObserver:
  void OnBrowserClosing(Browser* browser) override;

  const Browser* browser_;
  const scoped_refptr<const extensions::Extension> extension_;
  base::OnceClosure callback_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_REMOVAL_WATCHER_H_
