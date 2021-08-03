// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_entrypoints.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

TEST(LensEntryPointsTest, GetRegionSearchQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param =
      lens::GetQueryParameterFromEntryPoint(lens_region_search_ep);
  EXPECT_EQ(query_param, "ep=crs");
}

TEST(LensEntryPointsTest, GetImageSearchQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param =
      lens::GetQueryParameterFromEntryPoint(lens_image_search_ep);
  EXPECT_EQ(query_param, "ep=ccm");
}

TEST(LensEntryPointsTest, GetEmptyQueryParameterTest) {
  std::string query_param =
      lens::GetQueryParameterFromEntryPoint(lens::EntryPoint::UNKNOWN);
  EXPECT_EQ(query_param, "");
}

}  // namespace lens
