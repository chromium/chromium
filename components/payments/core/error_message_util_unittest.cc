// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/error_message_util.h"

#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

TEST(ErrorMessageUtilTest, GenerateKnownHttpStatusCodeError) {
  EXPECT_EQ(
      "Unable to download payment manifest \"https://example.test/\". HTTP 400 "
      "Bad Request.",
      GenerateHttpStatusCodeError(GURL("https://example.test"), 400));
}

TEST(ErrorMessageUtilTest, GenerateUnknownHttpStatusCodeError) {
  EXPECT_EQ(
      "Unable to download payment manifest \"https://example.test/\". HTTP 399 "
      "Unknown.",
      GenerateHttpStatusCodeError(GURL("https://example.test"), 399));
}

}  // namespace
}  // namespace payments
