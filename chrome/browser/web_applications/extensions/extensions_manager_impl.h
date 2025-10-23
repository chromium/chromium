// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_EXTENSIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_EXTENSIONS_MANAGER_IMPL_H_

#include <memory>
#include <unordered_set>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/extensions_manager.h"

class Profile;
class KeyedServiceBaseFactory;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class Extension;
class ExtensionRegistry;
class ExtensionService;
}  // namespace extensions

namespace web_app {

// TODO(http://crbug.com/454081171): Make tests for this implementation.
class ExtensionsManagerImpl : public ExtensionsManager {
 public:
  explicit ExtensionsManagerImpl(Profile* profile);
  ~ExtensionsManagerImpl() override;

  void OnExtensionSystemReady(base::OnceClosure on_ready) override;

  std::unordered_set<base::FilePath> GetIsolatedStoragePaths() override;

  // Creates an ExtensionInstallerGate which registers itself on
  // ExtensionService to delay Extension installs.
  std::unique_ptr<ExtensionInstallGate> RegisterGarbageCollectionInstallGate()
      override;

  static KeyedServiceBaseFactory* GetExtensionSystemSharedFactory();

 private:
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_EXTENSIONS_MANAGER_IMPL_H_
