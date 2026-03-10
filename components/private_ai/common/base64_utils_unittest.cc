// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/common/base64_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

TEST(Base64UtilsTest, ConvertBase64toBase64Url) {
  // Empty string is valid base64.
  EXPECT_EQ(
      ConvertBase64toBase64Url("", base::Base64UrlEncodePolicy::OMIT_PADDING),
      "");

  // Valid base64 strings
  EXPECT_EQ(ConvertBase64toBase64Url("YWJj",
                                     base::Base64UrlEncodePolicy::OMIT_PADDING),
            "YWJj");

  // Base64Url with - and _
  EXPECT_EQ(ConvertBase64toBase64Url("--__",
                                     base::Base64UrlEncodePolicy::OMIT_PADDING),
            "--__");

  // Base64Url with padding =
  EXPECT_EQ(ConvertBase64toBase64Url("YQ==",
                                     base::Base64UrlEncodePolicy::OMIT_PADDING),
            "YQ");

  EXPECT_EQ(ConvertBase64toBase64Url(
                "YQ==", base::Base64UrlEncodePolicy::INCLUDE_PADDING),
            "YQ==");

  // Invalid base64
  EXPECT_EQ(ConvertBase64toBase64Url("invalid^chars",
                                     base::Base64UrlEncodePolicy::OMIT_PADDING),
            std::nullopt);
}

}  // namespace private_ai
