// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/contextual_cue_log_event_helpers.h"

#include <vector>

#include "base/test/values_test_util.h"
#include "components/metrics/private_metrics/private_insights/events/contextual_cue_log_event.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

TEST(ContextualCueLogEventHelpersTest, SerializeEmptyList) {
  std::vector<events::ContextualCueLogEvent::PageInfo> empty_list;
  EXPECT_EQ("[]", SerializePageInfoListToJson(empty_list));
}

TEST(ContextualCueLogEventHelpersTest, SerializeOneItemList) {
  events::ContextualCueLogEvent::PageInfo page_info;
  page_info.set_url("http://url1.com");
  page_info.set_title("Title 1");
  std::vector<events::ContextualCueLogEvent::PageInfo> list = {page_info};
  EXPECT_THAT(SerializePageInfoListToJson(list),
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com\",\"title\":\"Title 1\"}]"));
}

TEST(ContextualCueLogEventHelpersTest, SerializeMultipleItemsList) {
  events::ContextualCueLogEvent::PageInfo page_info1;
  page_info1.set_url("http://url1.com");
  page_info1.set_title("Title 1");
  events::ContextualCueLogEvent::PageInfo page_info2;
  page_info2.set_url("http://url2.com");
  page_info2.set_title("Title 2");
  events::ContextualCueLogEvent::PageInfo page_info3;
  page_info3.set_url("http://url3.com");
  page_info3.set_title("Title 3");

  std::vector<events::ContextualCueLogEvent::PageInfo> list = {
      page_info1, page_info2, page_info3};
  EXPECT_THAT(SerializePageInfoListToJson(list),
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com\",\"title\":\"Title 1\"},"
                  "{\"url\":\"http://url2.com\",\"title\":\"Title 2\"},"
                  "{\"url\":\"http://url3.com\",\"title\":\"Title 3\"}]"));
}

TEST(ContextualCueLogEventHelpersTest, SerializeSpecialCharactersList) {
  events::ContextualCueLogEvent::PageInfo page_info1;
  page_info1.set_url("http://url1.com/a,b");
  page_info1.set_title("Title 1, with comma");
  events::ContextualCueLogEvent::PageInfo page_info2;
  page_info2.set_url("http://url2.com/\"quotes\"");
  page_info2.set_title("Title 2 \"with quotes\"");
  events::ContextualCueLogEvent::PageInfo page_info3;
  page_info3.set_url("http://url3.com/newline");
  page_info3.set_title("Title 3\nwith newline");

  std::vector<events::ContextualCueLogEvent::PageInfo> list = {
      page_info1, page_info2, page_info3};
  std::string json = SerializePageInfoListToJson(list);
  EXPECT_THAT(json,
              base::test::IsJson(
                  "[{\"url\":\"http://url1.com/a,b\",\"title\":\"Title 1, with "
                  "comma\"},"
                  "{\"url\":\"http://url2.com/"
                  "\\\"quotes\\\"\",\"title\":\"Title 2 \\\"with quotes\\\"\"},"
                  "{\"url\":\"http://url3.com/newline\",\"title\":\"Title "
                  "3\\nwith newline\"}]"));
}

}  // namespace private_insights
