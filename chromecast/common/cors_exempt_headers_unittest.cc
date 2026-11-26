// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cors_exempt_headers.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

TEST(CorsExemptHeadersTest, IsCorsExemptHeader) {
  EXPECT_TRUE(IsCorsExemptHeader("CAST-CERT"));
  EXPECT_TRUE(IsCorsExemptHeader("cast-cert"));
  EXPECT_TRUE(IsCorsExemptHeader("Authorization"));
  EXPECT_TRUE(IsCorsExemptHeader("authorization"));
  EXPECT_FALSE(IsCorsExemptHeader("TEST-HEADER"));
}

TEST(CorsExemptHeadersTest, GetCanonicalCorsExemptHeaderName) {
  EXPECT_EQ(GetCanonicalCorsExemptHeaderName("authorization"), "Authorization");
  EXPECT_EQ(GetCanonicalCorsExemptHeaderName("cast-jwt"), "CAST-JWT");
  EXPECT_EQ(GetCanonicalCorsExemptHeaderName("CAST-CERT"), "CAST-CERT");
  EXPECT_EQ(GetCanonicalCorsExemptHeaderName("TEST-HEADER"), std::nullopt);
}

}  // namespace
}  // namespace chromecast
