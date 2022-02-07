// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_provider.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class KeyboardShortcutProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    provider_ = std::make_unique<KeyboardShortcutProvider>(profile_.get());
    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<Profile> profile_;
  std::unique_ptr<KeyboardShortcutProvider> provider_;
};

TEST_F(KeyboardShortcutProviderTest, Search) {
  provider_->Start(u"loc");
  Wait();

  ASSERT_GT(provider_->results().size(), 0);
  EXPECT_EQ(provider_->results()[0]->title(), u"Lock screen");
  EXPECT_GT(provider_->results()[0]->relevance(), 0.8);
}

}  // namespace app_list
