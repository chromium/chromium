// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/grit/components_resources.h"
#include "components/resources/generate_about_credits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

using ::testing::HasSubstr;

TEST(ComponentsResourcesTests, AboutUiCreditsHtml) {
  std::string credits =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_ABOUT_UI_CREDITS_HTML);
  EXPECT_FALSE(credits.empty());

#if BUILDFLAG(GENERATE_ABOUT_CREDITS)
  // Spot-check that `tools/licenses/licenses.py` finds all the expected
  // `README.chromium` metadata.

  // `//third_party/rust/anyhow/v1/README.chromium`
  EXPECT_THAT(credits, HasSubstr("https://crates.io/crates/anyhow"));

  // `//third_party/brotli/README.chromium`
  EXPECT_THAT(credits, HasSubstr("https://github.com/google/brotli"));
#endif
}

}  // namespace
