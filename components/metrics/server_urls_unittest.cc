// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/server_urls.h"

#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

// Verify that on a branded build, the URLs are not empty (otherwise, metrics
// would not be sent). For a non-branded build, they should be empty.
TEST(MetricsServerUrls, GetUrls) {
  bool should_have_url = BUILDFLAG(GOOGLE_CHROME_BRANDING);

  EXPECT_EQ(GetMetricsServerUrl().is_valid(), should_have_url);
  EXPECT_EQ(GetInsecureMetricsServerUrl().is_valid(), should_have_url);
  EXPECT_EQ(GetCastMetricsServerUrl().is_valid(), should_have_url);
  EXPECT_EQ(GetUkmServerUrl().is_valid(), should_have_url);
  EXPECT_EQ(GetDwaServerUrl().is_valid(), should_have_url);
}

}  // namespace
}  // namespace metrics
