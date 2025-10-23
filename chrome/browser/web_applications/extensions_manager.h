// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_

#include <memory>
#include <unordered_set>

#include "base/functional/callback_forward.h"

class Profile;
class KeyedServiceBaseFactory;

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
class Extension;
class ExtensionService;
}  // namespace extensions

namespace web_app {

class ExtensionInstallGate {
 public:
  virtual ~ExtensionInstallGate() = default;
};

// This class wraps the extension system in a fakeable dependency, so our system
// doesn't have to directly depend on the extensions system (so no circular
// dependencies), and tests can fake this functionality without needing to use
// the whole Extensions system.
//
// TODO(http://crbug.com/454081171): Make tests for the implementation, and move
// all remaining extensions functionality the WebAppProvider system uses to this
// manager.
class ExtensionsManager {
 public:
  // Creates the 'real' implementation of this system for the given profile.
  static std::unique_ptr<ExtensionsManager> CreateForProfile(Profile*);
  static KeyedServiceBaseFactory* GetExtensionSystemSharedFactory();

  virtual ~ExtensionsManager() = default;

  // `on_ready` will be called when the extensions system is ready.
  virtual void OnExtensionSystemReady(base::OnceClosure on_ready) = 0;

  // Returns the isolated storage paths from the extensions system.
  virtual std::unordered_set<base::FilePath> GetIsolatedStoragePaths() = 0;

  // Creates an ExtensionInstallerGate which registers itself on
  // ExtensionService to delay Extension installs.
  virtual std::unique_ptr<ExtensionInstallGate>
  RegisterGarbageCollectionInstallGate() = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_
