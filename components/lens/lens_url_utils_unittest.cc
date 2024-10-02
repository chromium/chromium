// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_url_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_rendering_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::MatchesRegex;

namespace lens {

TEST(LensUrlUtilsTest, NonSidePanelRequestHasNoSidePanelSizeParams) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);

  // Despite passing in a nonzero size, there should not be any side panel
  // viewport size params.
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest,
     AppendOrReplaceViewportSizeForRequestSetsSidePanelSizeParams) {
  base::HistogramTester histogram_tester;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceViewportSizeForRequest(original_url,
                                                         gfx::Size(10, 10));
  EXPECT_THAT(url.query(), MatchesRegex("vpw=10&vph=10"));
  histogram_tester.ExpectBucketCount(
      "Search.Lens.ViewportDimensionsSent.Success", true, 1);
}

TEST(LensUrlUtilsTest,
     AppendOrReplaceViewportSizeForRequestOnlySetsNonZeroSizes) {
  base::HistogramTester histogram_tester;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url_with_height = lens::AppendOrReplaceViewportSizeForRequest(
      original_url, gfx::Size(0, 10));
  EXPECT_THAT(url_with_height.query(), MatchesRegex("vph=10"));
  GURL url_with_width = lens::AppendOrReplaceViewportSizeForRequest(
      original_url, gfx::Size(10, 0));
  EXPECT_THAT(url_with_width.query(), MatchesRegex("vpw=10"));
  GURL url_with_neither = lens::AppendOrReplaceViewportSizeForRequest(
      original_url, gfx::Size(0, 0));
  EXPECT_THAT(url_with_neither.query(), MatchesRegex(""));
  histogram_tester.ExpectBucketCount(
      "Search.Lens.ViewportDimensionsSent.Success", false, 3);
}

TEST(LensUrlUtilsTest, GetRegionSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_search_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=ccm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageTranslateNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_translate_ep = lens::EntryPoint::
      CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_translate_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=ctrcm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetRegionSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetRegionSearchCompanionQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_region_search_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false,
      /*is_companion_request=*/true);
  EXPECT_THAT(query_param,
              MatchesRegex("ep=crs&re=csc&s=4&source=chrome.gsc&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetCompanionRegionSearchQueryParameterTest) {
  lens::EntryPoint csc_region_search_ep =
      lens::EntryPoint::COMPANION_REGION_SEARCH;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      csc_region_search_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false,
      /*is_companion_request=*/true);
  EXPECT_THAT(query_param,
              MatchesRegex("ep=cscidr&re=csc&s=4&source=chrome.gsc&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_search_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=ccm&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageSearchCompanionQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_search_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false,
      /*is_companion_request=*/true);
  EXPECT_THAT(query_param,
              MatchesRegex("ep=ccm&re=csc&s=4&source=chrome.gsc&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageTranslateSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_translate_ep = lens::EntryPoint::
      CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_image_translate_ep, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=ctrcm&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetOpenNewTabSidePanelParameterTest) {
  lens::EntryPoint lens_open_new_tab_side_panel_ep =
      lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_open_new_tab_side_panel_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=cnts&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetFullscreenSearchQueryParameterTest) {
  lens::EntryPoint lens_ep =
      lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/true);
  EXPECT_THAT(query_param, MatchesRegex("ep=cfs&re=avsf&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetVideoFrameSearchQueryParameterTest) {
  auto lens_ep = lens::EntryPoint::CHROME_VIDEO_FRAME_SEARCH_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens_ep, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("ep=cvfs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetUnknownEntryPointTest) {
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN, /*is_lens_side_panel_request=*/false,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetUnknownEntryPointSidePanelTest) {
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN, /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendRegionSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_region_search_ep, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendImageSearchNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_search_ep, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ccm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendImageTranslateNewTabQueryParameterTest) {
  lens::EntryPoint lens_image_translate_ep = lens::EntryPoint::
      CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_translate_ep, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ctrcm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendRegionSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_region_search_ep, re,
      /*is_side_panel_request=*/true);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crs&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendImageSearchSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_search_ep, re,
      /*is_side_panel_request=*/true);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ccm&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendImageTranslateSidePanelQueryParameterTest) {
  lens::EntryPoint lens_image_translate_ep = lens::EntryPoint::
      CHROME_TRANSLATE_IMAGE_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_CHROME_SIDE_PANEL;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_translate_ep, re,
      /*is_side_panel_request=*/true);
  EXPECT_THAT(url.query(), MatchesRegex("ep=ctrcm&re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendOpenNewTabSidePanelParameterTest) {
  lens::EntryPoint lens_open_new_tab_side_panel_ep =
      lens::EntryPoint::CHROME_OPEN_NEW_TAB_SIDE_PANEL;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_open_new_tab_side_panel_ep, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=cnts&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendFullscreenSearchQueryParameterTest) {
  lens::EntryPoint lens_ep =
      lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_AMBIENT_VISUAL_SEARCH_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_ep, re, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=cfs&re=avsf&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendLensOverlayLocationBarQueryParameterTest) {
  lens::EntryPoint lens_ep = lens::EntryPoint::CHROME_LENS_OVERLAY_LOCATION_BAR;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_ep, re, /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crmntob&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendUnknownEntryPointTest) {
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens::EntryPoint::UNKNOWN, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendUnknownRenderingEnvironmentTest) {
  lens::EntryPoint ep = lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  GURL original_url = GURL("https://lens.google.com/");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, ep, lens::RenderingEnvironment::RENDERING_ENV_UNKNOWN,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("ep=crs&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendEmptyLogsTest) {
  std::vector<lens::mojom::LatencyLogPtr> log_data;
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN,
      /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("re=dcsp&s=4&st=\\d+"));
  lens::AppendLogsQueryParam(&query_param, std::move(log_data));
  EXPECT_THAT(query_param, MatchesRegex("re=dcsp&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendPopulatedLogsTest) {
  std::vector<lens::mojom::LatencyLogPtr> log_data;
  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::DOWNSCALE_START, gfx::Size(), gfx::Size(),
      lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
      /*encoded_size_bytes=*/0));
  std::string query_param = lens::GetQueryParametersForLensRequest(
      lens::EntryPoint::UNKNOWN,
      /*is_lens_side_panel_request=*/true,
      /*is_full_screen_request=*/false);
  EXPECT_THAT(query_param, MatchesRegex("re=dcsp&s=4&st=\\d+"));
  lens::AppendLogsQueryParam(&query_param, std::move(log_data));
  EXPECT_THAT(query_param, MatchesRegex("re=dcsp&s=4&st=\\d+&lm.+"));
}

TEST(LensUrlUtilsTest, AppendNonSidePanelSettingsRemovesViewportSizeTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  lens::RenderingEnvironment re =
      lens::RenderingEnvironment::ONELENS_DESKTOP_WEB_FULLSCREEN;
  GURL original_url =
      GURL("https://lens.google.com/search?p=123&vph=10&vpw=10");
  GURL url = lens::AppendOrReplaceQueryParametersForLensRequest(
      original_url, lens_image_search_ep, re,
      /*is_side_panel_request=*/false);
  EXPECT_THAT(url.query(), MatchesRegex("p=123&ep=ccm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendStartTimeUpdatesParamForLensUrlTest) {
  GURL original_url = GURL("https://lens.google.com/search?p=123");
  GURL url = lens::AppendOrReplaceStartTimeIfLensRequest(original_url);
  EXPECT_THAT(url.query(), MatchesRegex("p=123&st=\\d+"));
}

TEST(LensUrlUtilsTest, AppendStartTimeIgnoresNonLensUrlTest) {
  GURL original_url = GURL("https://not-lens.com/search?p=123");
  GURL url = lens::AppendOrReplaceStartTimeIfLensRequest(original_url);
  EXPECT_THAT(url.query(), MatchesRegex("p=123"));
}

}  // namespace lens
