// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/insecure_form_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using security_interstitials::IsInsecureFormActionOnSecureSource;

class InsecureFormUtilTest : public ::testing::Test {
 public:
#if BUILDFLAG(IS_IOS)
  void SetUp() override {
    security_interstitials::SetInsecureFormPortsForTesting(
        /*port_treated_as_secure=*/123,
        /*port_treated_as_insecure=*/456);
  }

  void TearDown() override {
    security_interstitials::SetInsecureFormPortsForTesting(
        /*port_treated_as_secure=*/0,
        /*port_treated_as_insecure=*/0);
  }
#endif
};

TEST_F(InsecureFormUtilTest, IsInsecureFormActionOnSecureSource) {
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(GURL("https://example.com"),
                                                 GURL("http://example.com")));

  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(GURL("http://example.com"),
                                                  GURL("http://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(GURL("http://example.com"),
                                                  GURL("https://example.com")));
  EXPECT_FALSE(IsInsecureFormActionOnSecureSource(GURL("https://example.com"),
                                                  GURL("https://example.com")));

#if BUILDFLAG(IS_IOS)
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(GURL("http://127.0.0.1:123"),
                                                 GURL("http://127.0.0.1:456")));
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(GURL("https://example.com"),
                                                 GURL("http://127.0.0.1:456")));
  EXPECT_TRUE(IsInsecureFormActionOnSecureSource(GURL("http://127.0.0.1:123"),
                                                 GURL("http://example.com")));
#endif
}
