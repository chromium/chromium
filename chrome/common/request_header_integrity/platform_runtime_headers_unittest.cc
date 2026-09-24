// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/platform_runtime_headers.h"

#include <string_view>

#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace request_header_integrity {

namespace {

net::HttpRequestHeaders MakeHeaders(std::string_view name,
                                    std::string_view value) {
  net::HttpRequestHeaders headers;
  headers.SetHeaderWithoutCheckForTesting(name, value);
  return headers;
}

class PlatformRuntimeHeadersTest : public testing::Test {
 protected:
  void SetUp() override {
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
  }
  void TearDown() override {
    PlatformRuntimeHeaders::GetInstance().ResetForTesting();
  }
};

}  // namespace

TEST_F(PlatformRuntimeHeadersTest, NoBlockLeavesRequestUntouched) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");

  EXPECT_EQ(PlatformRuntimeApplyResult::kUnavailable,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_EQ("original", headers.GetHeader("X-Test"));
}

TEST_F(PlatformRuntimeHeadersTest, OverwritesHeaderAlreadyPresent) {
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("X-Test", "computed"));

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");

  EXPECT_EQ(PlatformRuntimeApplyResult::kApplied,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_EQ("computed", headers.GetHeader("X-Test"));
}

TEST_F(PlatformRuntimeHeadersTest, DoesNotAddMissingHeader) {
  // The library only ever rewrites a header the request already carries.
  // Adding one here would change which requests are marked at all.
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("X-Test", "computed"));

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Other", "untouched");

  EXPECT_EQ(PlatformRuntimeApplyResult::kNotApplicable,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_FALSE(headers.HasHeader("X-Test"));
  EXPECT_EQ("untouched", headers.GetHeader("X-Other"));
}

TEST_F(PlatformRuntimeHeadersTest, HeaderNameMatchIsCaseInsensitive) {
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("x-test", "computed"));

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");

  EXPECT_EQ(PlatformRuntimeApplyResult::kApplied,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_EQ("computed", headers.GetHeader("X-Test"));
}

TEST_F(PlatformRuntimeHeadersTest, EmptyHeadersClearsHolder) {
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("X-Test", "computed"));
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");
  ASSERT_EQ(PlatformRuntimeApplyResult::kApplied,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));

  PlatformRuntimeHeaders::GetInstance().Set({});
  EXPECT_EQ(PlatformRuntimeApplyResult::kUnavailable,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
}

TEST_F(PlatformRuntimeHeadersTest, GarbageIsRejectedRatherThanStamped) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");

  PlatformRuntimeHeaders::GetInstance().Set(
      MakeHeaders("invalid header name with spaces", "value"));
  EXPECT_EQ(PlatformRuntimeApplyResult::kUnavailable,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));

  PlatformRuntimeHeaders::GetInstance().Set(
      MakeHeaders("X-Test", "invalid\r\nvalue"));
  EXPECT_EQ(PlatformRuntimeApplyResult::kUnavailable,
            PlatformRuntimeHeaders::GetInstance().Apply(&headers));
  EXPECT_EQ("original", headers.GetHeader("X-Test"));
}

TEST_F(PlatformRuntimeHeadersTest, LatestValueWins) {
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("X-Test", "first"));
  PlatformRuntimeHeaders::GetInstance().Set(MakeHeaders("X-Test", "second"));

  net::HttpRequestHeaders headers;
  headers.SetHeader("X-Test", "original");
  PlatformRuntimeHeaders::GetInstance().Apply(&headers);
  EXPECT_EQ("second", headers.GetHeader("X-Test"));
}

}  // namespace request_header_integrity
