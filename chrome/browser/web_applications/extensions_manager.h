// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_

#include <memory>
#include <unordered_set>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

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

class ExtensionInstallGate {
 public:
  virtual ~ExtensionInstallGate();
};

class ExtensionsManager {
 public:
  explicit ExtensionsManager(Profile* profile);
  virtual ~ExtensionsManager();

  virtual std::unordered_set<base::FilePath> GetIsolatedStoragePaths();

  // Returns ExtensionsPref::kStorageGarbageCollect which indicates possibly
  // deleted Storage Partitions on disk requiring garbage collection.
  // TODO(crbug.com/40922689): Delete ExtensionsPref::kStorageGarbageCollect.
  virtual bool ShouldGarbageCollectStoragePartitions();

  // Sets ExtensionsPref::kStorageGarbageCollect to false.
  virtual void ResetStorageGarbageCollectPref(base::OnceClosure callback);

  // Creates an ExtensionInstallerGate which registers itself on
  // ExtensionService to delay Extension installs.
  virtual std::unique_ptr<ExtensionInstallGate>
  RegisterGarbageCollectionInstallGate();

  static KeyedServiceBaseFactory* GetExtensionSystemSharedFactory();

 private:
  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<extensions::ExtensionRegistry> registry_ = nullptr;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_MANAGER_H_
