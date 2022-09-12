// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

#include "base/files/file_path.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class FakeBrowserContextHelperDelegate : public BrowserContextHelper::Delegate {
 public:
  content::BrowserContext* GetBrowserContextByPath(
      const base::FilePath& path) override {
    return nullptr;
  }
  content::BrowserContext* DeprecatedGetBrowserContext(
      const base::FilePath& path) override {
    return nullptr;
  }

  const base::FilePath* GetUserDataDir() override { return &user_data_dir_; }

 private:
  base::FilePath user_data_dir_{"user_data_dir"};
};

class BrowserContextHelperTest : public testing::Test {
 public:
  BrowserContextHelperTest() = default;
  ~BrowserContextHelperTest() override = default;

 private:
  // Sets up fake UI thread, required by TestBrowserContext.
  content::BrowserTaskEnvironment env_;
};

}  // namespace

TEST_F(BrowserContextHelperTest, GetUserIdHashFromBrowserContext) {
  // If nullptr is passed, returns an error.
  EXPECT_EQ("", BrowserContextHelper::GetUserIdHashFromBrowserContext(nullptr));

  constexpr struct {
    const char* expect;
    const char* path;
  } kTestData[] = {
      // Regular case. Use relative path, as temporary directory is created
      // there.
      {"abcde123456", "home/chronos/u-abcde123456"},

      // Special case for legacy path.
      {"user", "home/chronos/user"},

      // Special case for testing profile.
      {"test-user", "home/chronos/test-user"},

      // Error case. Data directory must start with "u-".
      {"", "abcde123456"},
  };
  for (const auto& test_case : kTestData) {
    content::TestBrowserContext context(base::FilePath(test_case.path));
    EXPECT_EQ(test_case.expect,
              BrowserContextHelper::GetUserIdHashFromBrowserContext(&context));
  }
}

TEST_F(BrowserContextHelperTest, GetUserBrowserContextDirName) {
  constexpr struct {
    const char* expect;
    const char* user_id_hash;
  } kTestData[] = {
      // Regular case.
      {"u-abcde123456", "abcde123456"},

      // Special case for the legacy path.
      {"user", "user"},

      // Special case for testing.
      {"test-user", "test-user"},
  };
  for (const auto& test_case : kTestData) {
    EXPECT_EQ(test_case.expect,
              BrowserContextHelper::GetUserBrowserContextDirName(
                  test_case.user_id_hash));
  }
}

TEST_F(BrowserContextHelperTest, GetBrowserContextPathByUserIdHash) {
  BrowserContextHelper helper(
      std::make_unique<FakeBrowserContextHelperDelegate>());
  // u- prefix is expected. See GetUserBrowserContextDirName for details.
  EXPECT_EQ(base::FilePath("user_data_dir/u-0123456789"),
            helper.GetBrowserContextPathByUserIdHash("0123456789"));
  // Special use name case.
  EXPECT_EQ(base::FilePath("user_data_dir/user"),
            helper.GetBrowserContextPathByUserIdHash("user"));
  EXPECT_EQ(base::FilePath("user_data_dir/test-user"),
            helper.GetBrowserContextPathByUserIdHash("test-user"));
}

}  // namespace ash
