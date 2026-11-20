// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/http_request.h"

#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater::test {

namespace {

using ::base::test::IsJson;
using ::testing::Optional;

// Returns a request with `content` as its decoded body.
HttpRequest MakeRequest(std::string content) {
  HttpRequest request;
  request.decoded_content = std::move(content);
  return request;
}

}  // namespace

TEST(HttpRequestTest, ParseJsonContentRequiresADict) {
  EXPECT_FALSE(ParseJsonContent(MakeRequest("")).has_value());
  EXPECT_FALSE(ParseJsonContent(MakeRequest("not valid json")).has_value());
  EXPECT_FALSE(ParseJsonContent(MakeRequest("[1, 2]")).has_value());
  EXPECT_THAT(ParseJsonContent(MakeRequest(R"({"request": 1})")),
              Optional(IsJson(R"({"request": 1})")));
}

TEST(HttpRequestTest, ParseJsonContentRenamesProtocolV3Names) {
  EXPECT_THAT(ParseJsonContent(MakeRequest(R"({"request": {
    "updater": [{"name": "x"}],
    "app": [
      {"appid": "a", "event": [{"eventtype": 3}], "events": [{"eventtype": 4}]},
      {"appid": "b", "events": [{"eventtype": 4}]},
      "not a dict"
    ]
  }})")),
              Optional(IsJson(R"({"request": {
    "updaters": [{"name": "x"}],
    "apps": [
      {"appid": "a", "events": [{"eventtype": 3}]},
      {"appid": "b"},
      "not a dict"
    ]
  }})")));
}

TEST(HttpRequestTest, ParseJsonContentKeepsV3NamesNextToV4Names) {
  // An `apps` list keeps the `event` lists of its apps.
  static constexpr char kContent[] = R"({"request": {
    "updaters": [{"name": "x"}],
    "updater": [{"name": "y"}],
    "apps": [{"appid": "a", "event": [{"eventtype": 3}]}],
    "app": [{"appid": "b"}]
  }})";
  EXPECT_THAT(ParseJsonContent(MakeRequest(kContent)),
              Optional(IsJson(kContent)));
}

TEST(HttpRequestTest, ParseJsonContentReplacesNonListApps) {
  EXPECT_THAT(ParseJsonContent(MakeRequest(R"({"request": {
    "apps": null,
    "app": [{"appid": "a", "event": [{"eventtype": 3}]}]
  }})")),
              Optional(IsJson(R"({"request": {
    "apps": [{"appid": "a", "events": [{"eventtype": 3}]}]
  }})")));
}

}  // namespace updater::test
