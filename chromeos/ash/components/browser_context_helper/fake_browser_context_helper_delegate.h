// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FAKE_BROWSER_CONTEXT_HELPER_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FAKE_BROWSER_CONTEXT_HELPER_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class TestBrowserContext;
}  // namespace content

namespace ash {

// Helper class for unit tests. Used to construct the BrowserContextHelper
// singleton.
class FakeBrowserContextHelperDelegate : public BrowserContextHelper::Delegate {
 public:
  FakeBrowserContextHelperDelegate();
  FakeBrowserContextHelperDelegate(const FakeBrowserContextHelperDelegate&) =
      delete;
  FakeBrowserContextHelperDelegate& operator=(
      const FakeBrowserContextHelperDelegate&) = delete;
  ~FakeBrowserContextHelperDelegate() override;

  // Creates a BrowserContext object for the given |path|. The |path| should
  // be a subpath of |user_data_dir_|.
  content::BrowserContext* CreateBrowserContext(const base::FilePath& path,
                                                bool is_off_the_record);

  // BrowserContextHelper::Delegate overrides.
  content::BrowserContext* GetBrowserContextByPath(
      const base::FilePath& path) override;
  content::BrowserContext* GetBrowserContextByAccountId(
      const AccountId& account_id) override;
  content::BrowserContext* DeprecatedGetBrowserContext(
      const base::FilePath& path) override;
  content::BrowserContext* GetOrCreatePrimaryOTRBrowserContext(
      content::BrowserContext* browser_context) override;
  content::BrowserContext* GetOriginalBrowserContext(
      content::BrowserContext* browser_context) override;
  const base::FilePath* GetUserDataDir() override;

 private:
  base::ScopedTempDir user_data_dir_;
  std::vector<std::unique_ptr<content::TestBrowserContext>>
      browser_context_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FAKE_BROWSER_CONTEXT_HELPER_DELEGATE_H_
