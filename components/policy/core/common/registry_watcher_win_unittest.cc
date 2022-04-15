// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_watcher_win.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr wchar_t kSubKeyToWatch[] = L"SOFTWARE\\Policies\\Chromium";
constexpr wchar_t kFieldName[] = L"ShowHomeButton";

class RegistryWatcherWinTest
    : public testing::TestWithParam<testing::tuple<bool, bool>> {
 public:
  RegistryWatcherWinTest(const RegistryWatcherWinTest&) = delete;
  RegistryWatcherWinTest& operator=(const RegistryWatcherWinTest&) = delete;

  void OnRegistryChanged() { std::move(run_loop_quit_closure_).Run(); }

 protected:
  RegistryWatcherWinTest() = default;
  ~RegistryWatcherWinTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  // Creates a registry key.
  static base::win::RegKey CreateKey(HKEY rootkey, const wchar_t* subkey) {
    return base::win::RegKey(rootkey, subkey, KEY_SET_VALUE);
  }

  // Write a value in a registry key.
  static bool WriteValue(HKEY rootkey,
                         const wchar_t* subkey,
                         const wchar_t* name,
                         DWORD in_value) {
    return CreateKey(rootkey, subkey).WriteValue(name, in_value) ==
           ERROR_SUCCESS;
  }

  void set_run_loop_quit_closure(base::OnceClosure closure) {
    run_loop_quit_closure_ = std::move(closure);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::OnceClosure run_loop_quit_closure_;
};

// A test condition is that the policy keys in both HKLM and HKCU exist. A
// callback should be called when a value in any keys is changed.
TEST_F(RegistryWatcherWinTest, DynamicRefresh) {
  std::vector<base::win::RegKey> keys_to_watch;
  static const HKEY kHives[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  // Create existing keys to watch.
  for (HKEY hive : kHives) {
    base::win::RegKey key_to_watch = CreateKey(hive, kSubKeyToWatch);
    ASSERT_TRUE(key_to_watch.Valid());
    keys_to_watch.push_back(std::move(key_to_watch));
  }

  // Start watching the keys.
  RegistryWatcherWin registry_key_watcher(kSubKeyToWatch);
  registry_key_watcher.StartWatching(base::BindRepeating(
      &RegistryWatcherWinTest::OnRegistryChanged, base::Unretained(this)));

  // Write a value in a key and see if the callback is invoked.
  for (auto& key : keys_to_watch) {
    base::RunLoop run_loop;
    set_run_loop_quit_closure(run_loop.QuitClosure());
    // Writing a value in the key must invoke the quit closure of the RunLoop.
    ASSERT_TRUE(key.WriteValue(kFieldName, 1) == ERROR_SUCCESS);
    run_loop.Run();
  }
}

// A test condition is that one of the policy keys, in HKLM or HKCU, is missing.
// A callback should be called when a value in an existing key is changed.
TEST_F(RegistryWatcherWinTest, DynamicRefreshWithSingleRootKey) {
  static const HKEY kHives[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (HKEY hive : kHives) {
    base::win::RegKey key_to_watch = CreateKey(hive, kSubKeyToWatch);
    ASSERT_TRUE(key_to_watch.Valid());

    base::RunLoop run_loop;
    RegistryWatcherWin registry_key_watcher(kSubKeyToWatch);
    registry_key_watcher.StartWatching(run_loop.QuitClosure());

    // Writing a value in the key must invoke the quit closure of the RunLoop.
    ASSERT_TRUE(key_to_watch.WriteValue(kFieldName, 1) == ERROR_SUCCESS);
    run_loop.Run();

    key_to_watch.DeleteKey(L"");
  }
}

INSTANTIATE_TEST_SUITE_P(MaybeCreateTest,
                         RegistryWatcherWinTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(RegistryWatcherWinTest, Create) {
  const bool is_dev_registry_key_supported = testing::get<0>(GetParam());
  const bool enable_registry_dynamic_refresh = testing::get<1>(GetParam());
  const bool should_be_blocked =
      is_dev_registry_key_supported && !enable_registry_dynamic_refresh;

  if (!enable_registry_dynamic_refresh) {
    ASSERT_TRUE(WriteValue(HKEY_LOCAL_MACHINE, kSubKeyToWatch,
                           kKeyRegistryDynamicRefreshEnabled, 0));
  }
  // Null will be returned if the dynamic refresh of policies from the Registry
  // is blocked.
  auto registry_watcher = RegistryWatcherWin::MaybeCreate(
      kSubKeyToWatch, is_dev_registry_key_supported);
  EXPECT_EQ(should_be_blocked, !registry_watcher);
}

}  // namespace policy
