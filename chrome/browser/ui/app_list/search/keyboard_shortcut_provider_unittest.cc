// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace app_list {

// Parameterized by feature ProductivityLauncher.
class KeyboardShortcutProviderTest
    : public testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  KeyboardShortcutProviderTest() {
    feature_list_.InitWithFeatureState(ash::features::kProductivityLauncher,
                                       GetParam());
  }

 protected:
  void SetUp() override {
    // A DCHECK inside a KSV metadata utility function relies on device lists
    // being complete.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    profile_ = std::make_unique<TestingProfile>();
    search_controller_ = std::make_unique<TestSearchController>();
    auto provider = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    provider_ = provider.get();
    search_controller_->AddProvider(0, std::move(provider));

    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  const SearchProvider::Results& results() {
    if (app_list_features::IsCategoricalSearchEnabled()) {
      return search_controller_->last_results();
    } else {
      return provider_->results();
    }
  }

  void StartSearch(const std::u16string& query) {
    search_controller_->StartSearch(query);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<TestSearchController> search_controller_;
  KeyboardShortcutProvider* provider_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         KeyboardShortcutProviderTest,
                         testing::Bool());

// Make search queries which yield shortcut results with shortcut key
// combinations of differing length and format. Check that the top result has a
// high relevance score, and correctly set title and accessible name.
TEST_P(KeyboardShortcutProviderTest, Search) {
  // Result format: Single Key
  StartSearch(u"overview mode");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Overview mode");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Overview mode, Shortcuts, Overview mode key");

  // Result format: Modifier + Key
  StartSearch(u"lock");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Lock screen");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Lock screen, Shortcuts, Search+ l");

  // Result format: Modifier1 + Modifier2 + Key
  StartSearch(u"previous tab");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Go to previous tab");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Go to previous tab, Shortcuts, Ctrl+ Shift+ Tab");

  // Result format: Modifier1 + Key1 or Modifier2 + Key2
  StartSearch(u"focus address");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Focus address bar");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Focus address bar, Shortcuts, Ctrl+ l or Alt+ d");

  // Result format: Custom template string which embeds a Modifier and a Key.
  StartSearch(u"switch quickly between windows");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Switch quickly between windows");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(
      results()[0]->accessible_name(),
      u"Switch quickly between windows, Shortcuts, Press and hold Alt, tap Tab "
      u"until you get to the window you want to open, then release.");

  // Result format: Special case result for Take screenshot/recording.
  StartSearch(u"take screenshot");
  Wait();

  ASSERT_FALSE(results().empty());
  EXPECT_EQ(results()[0]->title(), u"Take screenshot/recording");
  EXPECT_GT(results()[0]->relevance(), 0.8);
  EXPECT_EQ(results()[0]->accessible_name(),
            u"Take screenshot/recording, Shortcuts, Capture mode key or Ctrl+ "
            u"Shift+ Overview mode key");
}

}  // namespace app_list
