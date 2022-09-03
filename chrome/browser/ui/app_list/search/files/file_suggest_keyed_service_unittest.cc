// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/app_list/search/files/file_suggest_keyed_service.h"
#include "chrome/browser/ui/app_list/search/files/item_suggest_cache.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

class FileSuggestKeyedServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    file_suggest_service_ =
        std::make_unique<FileSuggestKeyedService>(profile_.get());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<FileSuggestKeyedService> file_suggest_service_;
};

TEST_F(FileSuggestKeyedServiceTest, GetSuggestData) {
  absl::optional<ItemSuggestCache::Results> results =
      file_suggest_service_->GetSuggestData(
          FileSuggestKeyedService::SuggestDataType::kItemSuggest);
  EXPECT_FALSE(results.has_value());
}

}  // namespace app_list
