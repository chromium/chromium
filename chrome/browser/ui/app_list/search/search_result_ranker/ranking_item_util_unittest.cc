// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using test::TestAppListControllerDelegate;
using testing::Eq;

namespace app_list {

class RankingItemUtilTest : public AppListTestBase {
 public:
  RankingItemUtilTest() {}
  ~RankingItemUtilTest() override {}

  // AppListTestBase overrides:
  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_controller_delegate_ =
        std::make_unique<::test::TestAppListControllerDelegate>();
  }

  void SetAdaptiveRankerParams(
      const std::map<std::string, std::string>& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        app_list_features::kEnableQueryBasedMixedTypesRanker, params);
  }

  std::unique_ptr<OmniboxResult> MakeOmniboxResult(
      AutocompleteMatchType::Type type) {
    AutocompleteMatch match;
    match.type = type;
    return std::make_unique<OmniboxResult>(profile_.get(),
                                           app_list_controller_delegate_.get(),
                                           nullptr, match, false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestAppListControllerDelegate> app_list_controller_delegate_;
};

TEST_F(RankingItemUtilTest, OmniboxSubtypeReturnedWithFinchParameterOn) {
  SetAdaptiveRankerParams({{"expand_omnibox_types", "true"}});
  std::unique_ptr<OmniboxResult> result =
      MakeOmniboxResult(AutocompleteMatchType::HISTORY_URL);
  RankingItemType type = RankingItemTypeFromSearchResult(*result.get());
  EXPECT_EQ(type, RankingItemType::kOmniboxHistory);
}

TEST_F(RankingItemUtilTest, SimplifyUrlId) {
  // Test handling different kinds of scheme, domain, and path. These should all
  // be no-ops.
  EXPECT_EQ(SimplifyUrlId("scheme://domain.com/path"),
            "scheme://domain.com/path");
  EXPECT_EQ(SimplifyUrlId("://domain.com"), "://domain.com");
  EXPECT_EQ(SimplifyUrlId("domain.com/path"), "domain.com/path");
  EXPECT_EQ(SimplifyUrlId("domain.com:1123/path"), "domain.com:1123/path");
  EXPECT_EQ(SimplifyUrlId("://"), "://");

  // Test removing trailing slash.
  EXPECT_EQ(SimplifyUrlId("scheme://domain.com/"), "scheme://domain.com");
  EXPECT_EQ(SimplifyUrlId("scheme:///"), "scheme://");
  EXPECT_EQ(SimplifyUrlId("scheme://"), "scheme://");

  // Test removing queries and fragments.
  EXPECT_EQ(SimplifyUrlId("domain.com/path?query=query"), "domain.com/path");
  EXPECT_EQ(SimplifyUrlId("scheme://path?query=query#fragment"),
            "scheme://path");
  EXPECT_EQ(SimplifyUrlId("scheme://?query=query#fragment"), "scheme://");
}

TEST_F(RankingItemUtilTest, SimplifyGoogleDocsUrlId) {
  EXPECT_EQ(SimplifyGoogleDocsUrlId("docs.google.com/hash/edit?"),
            "docs.google.com/hash");
  EXPECT_EQ(SimplifyGoogleDocsUrlId(
                "http://docs.google.com/hash/view?query#fragment"),
            "http://docs.google.com/hash");
  EXPECT_EQ(SimplifyGoogleDocsUrlId("https://docs.google.com/d/document/hash/"),
            "https://docs.google.com/d/document/hash");

  // We only want to remove one /view or /edit from the end of the URL.
  EXPECT_EQ(SimplifyGoogleDocsUrlId("docs.google.com/edit/hash/view/view"),
            "docs.google.com/edit/hash/view");
}

TEST_F(RankingItemUtilTest, NormalizeAppID) {
  const std::string raw_id = "mgndgikekgjfcpckkfioiadnlibdjbkf";
  const std::string id_with_scheme =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf";
  const std::string id_with_slash = "mgndgikekgjfcpckkfioiadnlibdjbkf/";
  const std::string id_with_scheme_and_slash =
      "chrome-extension://mgndgikekgjfcpckkfioiadnlibdjbkf/";

  EXPECT_EQ(NormalizeAppId(raw_id), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_scheme), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_slash), raw_id);
  EXPECT_EQ(NormalizeAppId(id_with_scheme_and_slash), raw_id);
}

}  // namespace app_list
