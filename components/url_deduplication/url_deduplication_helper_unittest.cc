// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_deduplication/url_deduplication_helper.h"

#include <memory>
#include <vector>

#include "base/strings/strcat.h"
#include "components/url_deduplication/deduplication_strategy.h"
#include "components/url_deduplication/url_strip_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace url_deduplication {

class MockURLStripHandler : public URLStripHandler {
 public:
  MockURLStripHandler() = default;
  MockURLStripHandler(const MockURLStripHandler&) = delete;
  MockURLStripHandler& operator=(const MockURLStripHandler&) = delete;
  ~MockURLStripHandler() override = default;

  MOCK_METHOD1(StripExtraParams, GURL(GURL url));
};

class URLDeduplicationHelperTest : public ::testing::Test {
 public:
  URLDeduplicationHelperTest() = default;

  void InitHelper(std::vector<std::unique_ptr<URLStripHandler>> handlers,
                  DeduplicationStrategy strategy) {
    helper_ =
        std::make_unique<URLDeduplicationHelper>(std::move(handlers), strategy);
  }

  URLDeduplicationHelper* Helper() { return helper_.get(); }

 private:
  std::unique_ptr<URLDeduplicationHelper> helper_;
};

const std::string kSamplePageTitle = "Sample page title";

TEST_F(URLDeduplicationHelperTest, StripURL) {
  GURL full_url = GURL(
      "https://www.foopayment.com:123?ref=foo"
      "#heading=h.xaresuk9ir9a&password=test1&username=test2?q=test");
  DeduplicationStrategy strategy;
  strategy.excluded_prefixes = {"www."};
  strategy.update_scheme = true;
  strategy.clear_username = true;
  strategy.clear_password = true;
  strategy.clear_query = true;
  strategy.clear_ref = true;
  strategy.clear_port = true;
  InitHelper({}, strategy);
  std::string stripped_url =
      Helper()->ComputeURLDeduplicationKey(full_url, kSamplePageTitle);
  ASSERT_EQ("http://foopayment.com/", stripped_url);
}

TEST_F(URLDeduplicationHelperTest, StripURLWithHandlers) {
  GURL full_url =
      GURL("https://www.google.com/search#heading=h.xaresuk9ir9a?q=test");
  DeduplicationStrategy strategy;
  auto handler1 = std::make_unique<MockURLStripHandler>();
  auto handler2 = std::make_unique<MockURLStripHandler>();
  EXPECT_CALL(*handler1, StripExtraParams(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [](GURL url) { return GURL("http://google.com/search"); }));
  EXPECT_CALL(*handler2, StripExtraParams(testing::_)).Times(0);
  std::vector<std::unique_ptr<URLStripHandler>> handlers;
  handlers.push_back(std::move(handler1));
  handlers.push_back(std::move(handler2));
  InitHelper(std::move(handlers), strategy);
  std::string stripped_url =
      Helper()->ComputeURLDeduplicationKey(full_url, kSamplePageTitle);
  ASSERT_EQ("http://google.com/search", stripped_url);
}

TEST_F(URLDeduplicationHelperTest, DeduplicateByDomainAndTitle) {
  DeduplicationStrategy strategy;
  strategy.clear_path = true;
  strategy.include_title = true;
  InitHelper({}, strategy);

  constexpr char kSampleCalendarPageTitle[] =
      "Google.com - Calendar - Week of Januaray 5, 2024";
  constexpr char kSampleBaseCalendarUrl[] = "https://calendar.google.com/";
  const std::string expected_dedup_url_key =
      base::StrCat({kSampleBaseCalendarUrl, "#", kSampleCalendarPageTitle});
  EXPECT_EQ(expected_dedup_url_key,
            Helper()->ComputeURLDeduplicationKey(
                GURL(base::StrCat({kSampleBaseCalendarUrl, "calendar/u/0/r"})),
                kSampleCalendarPageTitle));
  EXPECT_EQ(expected_dedup_url_key,
            Helper()->ComputeURLDeduplicationKey(
                GURL(base::StrCat(
                    {kSampleBaseCalendarUrl, "calendar/u/0/r/week/2024/1/05"})),
                kSampleCalendarPageTitle));
}

}  // namespace url_deduplication
