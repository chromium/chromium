// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_

#include <unordered_set>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/extensions_manager.h"

namespace web_app {

// This class can be used to 'fake' the ExtensionsManager in tests, which
// attempts to wrap the extensions dependency functionality used by the
// WebAppProvider system.
// TODO(http://crbug.com/454081171): Move tests to use this fake, and implement
// more of the functionality in the fake.
class FakeExtensionsManager : public ExtensionsManager {
 public:
  FakeExtensionsManager();
  ~FakeExtensionsManager() override;

  void SetExtensionsSytemReady(bool ready);
  void SetIsolatedStoragePaths(std::unordered_set<base::FilePath> paths);

  // ExtensionsManager:
  void OnExtensionSystemReady(base::OnceClosure) override;
  std::unordered_set<base::FilePath> GetIsolatedStoragePaths() override;
  std::unique_ptr<ExtensionInstallGate> RegisterGarbageCollectionInstallGate()
      override;

 private:
  bool extensions_system_ready_ = true;
  std::vector<base::OnceClosure> ready_waiters_;
  std::unordered_set<base::FilePath> isolated_storage_paths_;
};
}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_EXTENSIONS_MANAGER_H_
