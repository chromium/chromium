// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_utils.h"

#include "components/prefs/testing_pref_service.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace supervised_user {

class SupervisedUserUtilsTest : public ::testing::Test {
 public:
  SupervisedUserUtilsTest() {
    filter_.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());
  }

  ~SupervisedUserUtilsTest() override = default;

  supervised_user::SupervisedUserURLFilter& filter() { return filter_; }

 private:
  TestingPrefServiceSimple pref_service_;
  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          pref_service_,
          std::make_unique<FakeURLFilterDelegate>());
};

TEST_F(SupervisedUserUtilsTest, StripOnDefaultFilteringBehaviour) {
  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::DEFAULT;
  supervised_user::UrlFormatter url_formatter(filter(), reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       StripOnManualFilteringBehaviourWithoutConflict) {
  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::MANUAL;
  supervised_user::UrlFormatter url_formatter(filter(), reason);

  GURL full_url("http://www.example.com");
  GURL stripped_url("http://example.com");

  EXPECT_EQ(stripped_url, url_formatter.FormatUrl(full_url));
}

TEST_F(SupervisedUserUtilsTest,
       SkipStripOnManualFilteringBehaviourWithConflict) {
  supervised_user::FilteringBehaviorReason reason =
      supervised_user::FilteringBehaviorReason::MANUAL;
  GURL full_url("http://www.example.com");

  // Add an conflicting entry in the blocklist.
  std::map<std::string, bool> url_map;
  url_map.emplace(full_url.host(), false);
  filter().SetManualHosts(url_map);
  supervised_user::UrlFormatter url_formatter(filter(), reason);

  EXPECT_EQ(full_url, url_formatter.FormatUrl(full_url));
}

}  // namespace supervised_user
