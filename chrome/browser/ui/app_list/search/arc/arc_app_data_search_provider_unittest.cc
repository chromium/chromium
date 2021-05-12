// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_provider.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"

namespace app_list {

class ArcAppDataSearchProviderTest : public AppListTestBase {
 protected:
  ArcAppDataSearchProviderTest() = default;
  ~ArcAppDataSearchProviderTest() override = default;

  // AppListTestBase:
  void SetUp() override {
    AppListTestBase::SetUp();
    arc_test_.SetUp(profile());
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
  }

  void TearDown() override {
    controller_.reset();
    arc_test_.TearDown();
    AppListTestBase::TearDown();
  }

  std::unique_ptr<ArcAppDataSearchProvider> CreateSearch(int max_results) {
    return std::make_unique<ArcAppDataSearchProvider>(max_results,
                                                      controller_.get());
  }

 private:
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDataSearchProviderTest);
};

TEST_F(ArcAppDataSearchProviderTest, Basic) {
  constexpr size_t kMaxResults = 6;
  constexpr char16_t kQuery[] = u"App Data Search";

  std::unique_ptr<ArcAppDataSearchProvider> provider =
      CreateSearch(kMaxResults);
  EXPECT_TRUE(provider->results().empty());
  arc::IconDecodeRequest::DisableSafeDecodingForTesting();

  provider->Start(kQuery);
  const auto& results = provider->results();
  EXPECT_EQ(2u, results.size());
  // Verify Person search result.
  int i = 0;
  EXPECT_EQ(base::StrCat({u"Label ", kQuery, u" ", base::NumberToString16(i)}),
            results[i]->title());
  EXPECT_EQ(ash::SearchResultDisplayType::kTile, results[i]->display_type());
  EXPECT_TRUE(results[i]->details().empty());
  // Verify Note document search result.
  ++i;
  EXPECT_EQ(base::StrCat({u"Label ", kQuery, u" ", base::NumberToString16(i)}),
            results[i]->title());
  EXPECT_EQ(ash::SearchResultDisplayType::kList, results[i]->display_type());
  EXPECT_EQ(base::StrCat({u"Text ", kQuery, u" ", base::NumberToString16(i)}),
            results[i]->details());
}

}  // namespace app_list
