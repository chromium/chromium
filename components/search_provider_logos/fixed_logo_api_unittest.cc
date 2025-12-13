// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/fixed_logo_api.h"

#include <string>

#include "components/search_provider_logos/logo_common.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_provider_logos {

TEST(FixedLogoApiTest, TestBasicParse) {
  const std::string response = "test";

  bool failed = false;
  std::unique_ptr<EncodedLogo> logo =
      ParseFixedLogoResponse(response, base::Time(), &failed);

  ASSERT_FALSE(failed);
  ASSERT_TRUE(logo);
  EXPECT_EQ(LogoType::LOGO, logo->metadata.type);
  EXPECT_TRUE(logo->metadata.can_show_after_expiration);
}

}  // namespace search_provider_logos
