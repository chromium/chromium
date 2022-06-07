// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_entrypoints.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::MatchesRegex;

namespace lens {

TEST(LensEntryPointsTest, GetRegionSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, GetImageSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_search_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=ccm&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, GetRegionSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_side_panel_request=*/true);
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&s=csp&st=\\d+"));
}

TEST(LensEntryPointsTest, GetImageSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_search_ep, /*is_side_panel_request=*/true);
  EXPECT_THAT(query_param, MatchesRegex("ep=ccm&s=csp&st=\\d+"));
}

TEST(LensEntryPointsTest, GetOpenNewTabSidePanelParameterTest) {
  lens::EntryPoint lens_open_new_tab_side_panel_ep =
      lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_open_new_tab_side_panel_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=cnts&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, GetFullscreenSearchQueryParameterTest) {
  lens::EntryPoint lens_ep =
      lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=cfs&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, GetUnknownEntryPointTest) {
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN, /*is_side_panel_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("s=&st=\\d+"));
}

TEST(LensEntryPointsTest, GetUnknownEntryPointSidePanelTest) {
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN, /*is_side_panel_request=*/true);
  EXPECT_THAT(query_param, MatchesRegex("s=csp&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendRegionSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_region_search_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crs&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendImageSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_search_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ccm&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendRegionSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_region_search_ep, /*is_side_panel_request=*/true);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crs&s=csp&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendImageSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_search_ep, /*is_side_panel_request=*/true);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ccm&s=csp&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendOpenNewTabSidePanelParameterTest) {
  lens::EntryPoint lens_open_new_tab_side_panel_ep =
      lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_open_new_tab_side_panel_ep,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=cnts&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendFullscreenSearchQueryParameterTest) {
  lens::EntryPoint lens_ep =
      lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_ep, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=cfs&s=&st=\\d+"));
}

TEST(LensEntryPointsTest, AppendUnknownEntryPointTest) {
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens::EntryPoint::UNKNOWN, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("s=&st=\\d+"));
}

}  // namespace lens
