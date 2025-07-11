// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils.h"

#include <memory>
#include <string_view>

#include "base/strings/strcat.h"
#include "components/dom_distiller/core/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace dom_distiller {

class DistillablePageUtilsTest : public testing::Test {};

TEST_F(DistillablePageUtilsTest, TestDistillabilityResultEquals_SameUrl) {
  DistillabilityResult result_1, result_2;
  result_1.url = result_2.url = GURL("https://foo.com");
  result_1.is_distillable = result_2.is_distillable = true;
  result_1.is_last = result_2.is_last = true;
  result_1.is_long_article = result_2.is_long_article = true;
  result_1.is_mobile_friendly = result_2.is_mobile_friendly = true;
  EXPECT_TRUE(result_1 == result_2);
}

TEST_F(DistillablePageUtilsTest, TestDistillabilityResultEquals_DiffUrl) {
  DistillabilityResult result_1, result_2;
  result_1.url = GURL("https://foo.com");
  result_2.url = GURL("https://bar.com");
  result_1.is_distillable = result_2.is_distillable = true;
  result_1.is_last = result_2.is_last = true;
  result_1.is_long_article = result_2.is_long_article = true;
  result_1.is_mobile_friendly = result_2.is_mobile_friendly = true;
  EXPECT_FALSE(result_1 == result_2);
}

}  // namespace dom_distiller
