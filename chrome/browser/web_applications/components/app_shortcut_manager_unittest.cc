// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind_test_util.h"
#include "chrome/browser/web_applications/components/app_shortcut_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_app_shortcut_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

class TestShortcutObserver : public AppShortcutObserver {
 public:
  TestShortcutObserver() {}

  void OnShortcutsCreated(const AppId& app_id) override {
    on_shortcuts_created_calls_++;
  }

  size_t on_shortcuts_created_calls() const {
    return on_shortcuts_created_calls_;
  }

 private:
  size_t on_shortcuts_created_calls_ = 0;
};

}  // namespace

constexpr char kAppId[] = "app-id";

class AppShortcutManagerTest : public WebAppTest {
 protected:
  AppShortcutManagerTest() {}

  void SetUp() override {
    WebAppTest::SetUp();

    shortcut_manager_ = std::make_unique<TestAppShortcutManager>(profile());
    shortcut_observer_ = std::make_unique<TestShortcutObserver>();

    shortcut_manager_->AddObserver(shortcut_observer_.get());
  }

  void TearDown() override {
    WebAppTest::TearDown();

    shortcut_manager_->RemoveObserver(shortcut_observer_.get());
  }

  TestAppShortcutManager& shortcut_manager() {
    return *shortcut_manager_.get();
  }

  TestShortcutObserver& shortcut_observer() {
    return *shortcut_observer_.get();
  }

  void CreateShortcuts(const AppId& app_id, bool add_to_desktop) {
    base::RunLoop loop;
    shortcut_manager().CreateShortcuts(
        app_id, add_to_desktop,
        base::BindLambdaForTesting([&loop](bool /*success*/) { loop.Quit(); }));
    loop.Run();
  }

 private:
  std::unique_ptr<TestAppShortcutManager> shortcut_manager_;
  std::unique_ptr<TestShortcutObserver> shortcut_observer_;
};

TEST_F(AppShortcutManagerTest, OnShortcutsCreatedFiresOnSuccess) {
  shortcut_manager().SetNextCreateShortcutsResult(kAppId, /*success=*/true);

  CreateShortcuts(kAppId, true);

  EXPECT_EQ(1u, shortcut_manager().num_create_shortcuts_calls());
  EXPECT_EQ(1u, shortcut_observer().on_shortcuts_created_calls());
}

TEST_F(AppShortcutManagerTest, OnShortcutsCreatedDoesNotFireOnFailure) {
  shortcut_manager().SetNextCreateShortcutsResult(kAppId, /*success=*/false);

  CreateShortcuts(kAppId, true);

  EXPECT_EQ(1u, shortcut_manager().num_create_shortcuts_calls());
  EXPECT_EQ(0u, shortcut_observer().on_shortcuts_created_calls());
}

}  // namespace web_app
