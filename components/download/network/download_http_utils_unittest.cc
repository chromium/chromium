// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/network/download_http_utils.h"

#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

TEST(DownloadHttpUtils, ParseRangeHeader) {
  net::HttpRequestHeaders request_headers;
  auto byte_range = ParseRangeHeader(request_headers);
  EXPECT_FALSE(byte_range.has_value());

  request_headers.AddHeaderFromString("Range: bytes=5-10");
  byte_range = ParseRangeHeader(request_headers);
  EXPECT_EQ(5, byte_range->first_byte_position());
  EXPECT_EQ(10, byte_range->last_byte_position());

  request_headers.Clear();
  request_headers.AddHeaderFromString("Range: bytes=5-");
  byte_range = ParseRangeHeader(request_headers);
  EXPECT_EQ(5, byte_range->first_byte_position());
  EXPECT_EQ(-1, byte_range->last_byte_position());

  request_headers.Clear();
  request_headers.AddHeaderFromString("Range: bytes=-5");
  byte_range = ParseRangeHeader(request_headers);
  EXPECT_TRUE(byte_range->IsSuffixByteRange());
  EXPECT_FALSE(byte_range->HasFirstBytePosition());
  EXPECT_FALSE(byte_range->HasLastBytePosition());
  EXPECT_EQ(5, byte_range->suffix_length());

  request_headers.Clear();
  request_headers.AddHeaderFromString("Range: bytes=5-10, 11-12");
  byte_range = ParseRangeHeader(request_headers);
  EXPECT_FALSE(byte_range.has_value()) << "Multiple range are not supported.";
}

TEST(DownloadHttpUtils, ValidateRequestHeaders) {
  net::HttpRequestHeaders request_headers;
  EXPECT_TRUE(ValidateRequestHeaders(request_headers));
  request_headers.AddHeaderFromString("Range: bytes=5-10");
  EXPECT_TRUE(ValidateRequestHeaders(request_headers));

  request_headers.Clear();
  request_headers.AddHeaderFromString("Range: kales=5-10");
  EXPECT_FALSE(ValidateRequestHeaders(request_headers));
}

}  // namespace
}  // namespace download