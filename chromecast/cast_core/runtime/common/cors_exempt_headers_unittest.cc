// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cors_exempt_headers.h"

#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Contains;
using ::testing::IsSupersetOf;
using ::testing::SizeIs;

namespace chromecast {

TEST(CorsExemptHeadersTest, GetCorsExemptHeadersSucceeds) {
  // Cast Core adds an additional header, Accept-Language, so size is one more.
  EXPECT_THAT(GetCastCoreCorsExemptHeadersList(),
              SizeIs(GetLegacyCorsExemptHeaders().size() + 1));
  EXPECT_THAT(GetCastCoreCorsExemptHeadersList(),
              IsSupersetOf(GetLegacyCorsExemptHeaders()));
  EXPECT_THAT(GetCastCoreCorsExemptHeadersList(), Contains("Accept-Language"));
}

TEST(CorsExemptHeadersTest, IsHeaderCorsExemptSucceeds) {
  EXPECT_TRUE(IsHeaderCorsExempt("Accept-Language"));
  EXPECT_TRUE(IsHeaderCorsExempt("aCCepT-lanGUAGe"));
  EXPECT_FALSE(IsHeaderCorsExempt("TEST-NONCORS-HEADER"));
}

}  // namespace chromecast
