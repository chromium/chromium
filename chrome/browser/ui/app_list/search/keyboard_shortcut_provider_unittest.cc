// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_provider.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace app_list {

class KeyboardShortcutProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    // A DCHECK inside a KSV metadata utility function relies on device lists
    // being complete.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    profile_ = std::make_unique<TestingProfile>();
    provider_ = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<KeyboardShortcutProvider> provider_;
};

// Make search queries which yield shortcut results with shortcut key
// combinations of differing length and format. Check that the top result has a
// high relevance score, and correctly set title and accessible name.
TEST_F(KeyboardShortcutProviderTest, Search) {
  // Result format: Single Key
  provider_->Start(u"take screenshot");
  Wait();

  ASSERT_FALSE(provider_->results().empty());
  EXPECT_EQ(provider_->results()[0]->title(), u"Take screenshot/recording");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
  EXPECT_EQ(provider_->results()[0]->accessible_name(),
            u"Take screenshot/recording, Shortcuts, Capture mode key");

  // Result format: Modifier + Key
  provider_->Start(u"loc");
  Wait();

  ASSERT_FALSE(provider_->results().empty());
  EXPECT_EQ(provider_->results()[0]->title(), u"Lock screen");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
  EXPECT_EQ(provider_->results()[0]->accessible_name(),
            u"Lock screen, Shortcuts, Search+ l");

  // Result format: Modifier1 + Modifier2 + Key
  provider_->Start(u"previous tab");
  Wait();

  ASSERT_FALSE(provider_->results().empty());
  EXPECT_EQ(provider_->results()[0]->title(), u"Go to previous tab");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
  EXPECT_EQ(provider_->results()[0]->accessible_name(),
            u"Go to previous tab, Shortcuts, Ctrl+ Shift+ Tab");

  // Result format: Modifier1 + Key1 or Modifier2 + Key2
  provider_->Start(u"focus address");
  Wait();

  ASSERT_FALSE(provider_->results().empty());
  EXPECT_EQ(provider_->results()[0]->title(), u"Focus address bar");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
  EXPECT_EQ(provider_->results()[0]->accessible_name(),
            u"Focus address bar, Shortcuts, Ctrl+ l or Alt+ d");

  // Result format: Custom template string which embeds a Modifier and a Key.
  provider_->Start(u"switch quickly between windows");
  Wait();

  ASSERT_FALSE(provider_->results().empty());
  EXPECT_EQ(provider_->results()[0]->title(),
            u"Switch quickly between windows");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
  EXPECT_EQ(
      provider_->results()[0]->accessible_name(),
      u"Switch quickly between windows, Shortcuts, Press and hold Alt, tap Tab "
      u"until you get to the window you want to open, then release.");
}

}  // namespace app_list
