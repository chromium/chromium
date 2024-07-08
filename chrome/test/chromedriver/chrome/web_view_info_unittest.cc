// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/web_view_info.h"

#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

testing::AssertionResult StatusOk(const Status& status) {
  return StatusCodeIs<kOk>(status);
}

}  // namespace

class ParseSupportedTypeTest
    : public testing::TestWithParam<std::pair<std::string, WebViewInfo::Type>> {
 public:
  const std::string& TypeString() const { return GetParam().first; }

  WebViewInfo::Type Type() const { return GetParam().second; }
};

TEST_P(ParseSupportedTypeTest, ParseType) {
  std::string text = TypeString();
  const WebViewInfo::Type expected_type = Type();
  WebViewInfo::Type actual_type;
  EXPECT_TRUE(StatusOk(WebViewInfo::ParseType(text, actual_type)));
  EXPECT_EQ(expected_type, actual_type);
}

INSTANTIATE_TEST_SUITE_P(
    WebViewInfo,
    ParseSupportedTypeTest,
    ::testing::Values(
        std::make_pair("app", WebViewInfo::kApp),
        std::make_pair("background_page", WebViewInfo::kBackgroundPage),
        std::make_pair("browser", WebViewInfo::kBrowser),
        std::make_pair("external", WebViewInfo::kExternal),
        std::make_pair("iframe", WebViewInfo::kIFrame),
        std::make_pair("other", WebViewInfo::kOther),
        std::make_pair("page", WebViewInfo::kPage),
        std::make_pair("service_worker", WebViewInfo::kServiceWorker),
        std::make_pair("shared_worker", WebViewInfo::kSharedWorker),
        std::make_pair("webview", WebViewInfo::kWebView),
        std::make_pair("worker", WebViewInfo::kWorker)));

class ParseUnsupportedTypeTest : public testing::TestWithParam<std::string> {
 public:
  const std::string& TypeString() const { return GetParam(); }
};

TEST_P(ParseUnsupportedTypeTest, ParseType) {
  WebViewInfo::Type actual_type;
  EXPECT_TRUE(StatusOk(WebViewInfo::ParseType(TypeString(), actual_type)));
  EXPECT_EQ(WebViewInfo::kOther, actual_type);
}

INSTANTIATE_TEST_SUITE_P(WebViewInfo,
                         ParseUnsupportedTypeTest,
                         ::testing::Values("auction_worklet",
                                           "shared_storage_worklet",
                                           "unknown"));

TEST(WebViewInfo, ParseEmptyType) {
  WebViewInfo::Type actual_type;
  EXPECT_TRUE(
      StatusCodeIs<kUnknownError>(WebViewInfo::ParseType("", actual_type)));
}
