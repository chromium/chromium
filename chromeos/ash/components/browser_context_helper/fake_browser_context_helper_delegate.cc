// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/fake_browser_context_helper_delegate.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_context.h"

namespace ash {

FakeBrowserContextHelperDelegate::FakeBrowserContextHelperDelegate() {
  CHECK(user_data_dir_.CreateUniqueTempDir());
}

FakeBrowserContextHelperDelegate::~FakeBrowserContextHelperDelegate() = default;

content::BrowserContext* FakeBrowserContextHelperDelegate::CreateBrowserContext(
    const base::FilePath& path,
    bool is_off_the_record) {
  CHECK(user_data_dir_.GetPath().IsParent(path));

  auto browser_context = std::make_unique<content::TestBrowserContext>(path);
  browser_context->set_is_off_the_record(is_off_the_record);
  auto* browser_context_ptr = browser_context.get();
  browser_context_list_.push_back(std::move(browser_context));
  return browser_context_ptr;
}

content::BrowserContext*
FakeBrowserContextHelperDelegate::GetBrowserContextByPath(
    const base::FilePath& path) {
  for (auto& candidate : browser_context_list_) {
    if (candidate->GetPath() == path && !candidate->IsOffTheRecord()) {
      return candidate.get();
    }
  }
  return nullptr;
}

content::BrowserContext*
FakeBrowserContextHelperDelegate::GetBrowserContextByAccountId(
    const AccountId& account_id) {
  auto it = base::ranges::find_if(
      browser_context_list_, [&account_id](const auto& candidate) {
        auto* annotated_id = AnnotatedAccountId::Get(candidate.get());
        return annotated_id && *annotated_id == account_id;
      });
  return it != browser_context_list_.end() ? it->get() : nullptr;
}

content::BrowserContext*
FakeBrowserContextHelperDelegate::DeprecatedGetBrowserContext(
    const base::FilePath& path) {
  auto* browser_context = GetBrowserContextByPath(path);
  if (browser_context) {
    return nullptr;
  }

  return CreateBrowserContext(path, /*is_off_the_record=*/false);
}

content::BrowserContext*
FakeBrowserContextHelperDelegate::GetOrCreatePrimaryOTRBrowserContext(
    content::BrowserContext* browser_context) {
  const auto& path = browser_context->GetPath();
  for (auto& candidate : browser_context_list_) {
    if (candidate.get() != browser_context && candidate->GetPath() == path &&
        candidate->IsOffTheRecord()) {
      return candidate.get();
    }
  }
  return CreateBrowserContext(path, /*is_off_the_record=*/true);
}

content::BrowserContext*
FakeBrowserContextHelperDelegate::GetOriginalBrowserContext(
    content::BrowserContext* browser_context) {
  return GetBrowserContextByPath(browser_context->GetPath());
}

const base::FilePath* FakeBrowserContextHelperDelegate::GetUserDataDir() {
  return &user_data_dir_.GetPath();
}

}  // namespace ash
