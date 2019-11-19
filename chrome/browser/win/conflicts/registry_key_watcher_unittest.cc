// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/registry_key_watcher.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

class RegistryKeyWatcherTest : public testing::Test {
 protected:
  RegistryKeyWatcherTest() = default;
  ~RegistryKeyWatcherTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  // Creates a registry key.
  static bool CreateKey(HKEY root, const wchar_t* subkey) {
    return base::win::RegKey(root, subkey, KEY_SET_VALUE).Valid();
  }

  // Deletes a registry key.
  static bool DeleteKey(HKEY root, const wchar_t* subkey) {
    base::win::RegKey registry_key(root);
    return registry_key.Valid() &&
           registry_key.DeleteKey(subkey) == ERROR_SUCCESS;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(RegistryKeyWatcherTest);
};

TEST_F(RegistryKeyWatcherTest, InvalidKey) {
  auto registry_key_watcher = RegistryKeyWatcher::Create(
      HKEY_CURRENT_USER, L"Foo\\Bar", 0, base::OnceClosure());
  EXPECT_FALSE(registry_key_watcher);
}

TEST_F(RegistryKeyWatcherTest, WatchKeyDeletion) {
  static constexpr wchar_t kRegistryKeyPath[] = L"Foo\\Bar";

  // Create an existing key to watch.
  ASSERT_TRUE(CreateKey(HKEY_CURRENT_USER, kRegistryKeyPath));

  base::RunLoop run_loop;
  auto registry_key_watcher = RegistryKeyWatcher::Create(
      HKEY_CURRENT_USER, kRegistryKeyPath, 0, run_loop.QuitClosure());
  EXPECT_TRUE(registry_key_watcher);

  // Deleting the key must invoke the quit closure of the RunLoop.
  ASSERT_TRUE(DeleteKey(HKEY_CURRENT_USER, kRegistryKeyPath));
  run_loop.Run();
}
