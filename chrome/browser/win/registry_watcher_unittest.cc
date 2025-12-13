// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/registry_watcher.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::wstring kTestKeyPath1 =
    L"Software\\ChromiumTests\\RegistryWatcherTest1";
const std::wstring kTestKeyPath2 =
    L"Software\\ChromiumTests\\RegistryWatcherTest2";

void CreateRegistryKey(std::wstring_view path) {
  base::win::RegKey key(HKEY_CURRENT_USER, path.data(),
                        KEY_WRITE | KEY_CREATE_SUB_KEY);
  ASSERT_TRUE(key.Valid());
}

void ModifyRegistryKey(std::wstring_view path) {
  base::win::RegKey key(HKEY_CURRENT_USER, path.data(), KEY_WRITE);
  ASSERT_TRUE(key.Valid());
  ASSERT_EQ(key.WriteValue(L"TestValue", L"data"), ERROR_SUCCESS);
}

}  // namespace

class RegistryWatcherTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(RegistryWatcherTest, CallbackFiresOnSingleKeyChange) {
  const std::vector<std::wstring> kKeyPaths = {kTestKeyPath1};
  CreateRegistryKey(kTestKeyPath1);

  base::test::TestFuture<void> future;
  RegistryWatcher watcher(kKeyPaths, future.GetCallback());

  ModifyRegistryKey(kTestKeyPath1);

  EXPECT_TRUE(future.Wait());
}

TEST_F(RegistryWatcherTest, CallbackFiresOnAnyOfMultipleKeysChange) {
  const std::vector<std::wstring> kKeyPaths = {kTestKeyPath1, kTestKeyPath2};
  CreateRegistryKey(kTestKeyPath1);
  CreateRegistryKey(kTestKeyPath2);

  base::test::TestFuture<void> future;
  RegistryWatcher watcher(kKeyPaths, future.GetCallback());

  ModifyRegistryKey(kTestKeyPath2);

  EXPECT_TRUE(future.Wait());
}

TEST_F(RegistryWatcherTest, CallbackDoesNotFireWithoutChange) {
  const std::vector<std::wstring> kKeyPaths = {kTestKeyPath1};
  CreateRegistryKey(kTestKeyPath1);

  base::test::TestFuture<void> future;
  RegistryWatcher watcher(kKeyPaths, future.GetCallback());

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(future.IsReady());
}
