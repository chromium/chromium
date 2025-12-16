// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_url_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/lens/lens_entrypoints.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::MatchesRegex;

namespace lens {

TEST(LensUrlUtilsTest, GetRegionSearchParameterTest) {
  lens::EntryPoint lens_region_search_ep =
      lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  std::string query_param =
      lens::GetQueryParametersForLensRequest(lens_region_search_ep);
  EXPECT_THAT(query_param, MatchesRegex("ep=crs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetImageSearcParameterTest) {
  lens::EntryPoint lens_image_search_ep =
      lens::EntryPoint::CHROME_SEARCH_WITH_GOOGLE_LENS_CONTEXT_MENU_ITEM;
  std::string query_param =
      lens::GetQueryParametersForLensRequest(lens_image_search_ep);
  EXPECT_THAT(query_param, MatchesRegex("ep=ccm&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetVideoFrameSearchQueryParameterTest) {
  auto lens_ep = lens::EntryPoint::CHROME_VIDEO_FRAME_SEARCH_CONTEXT_MENU_ITEM;
  std::string query_param = lens::GetQueryParametersForLensRequest(lens_ep);
  EXPECT_THAT(query_param, MatchesRegex("ep=cvfs&re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, GetUnknownEntryPointTest) {
  std::string query_param =
      lens::GetQueryParametersForLensRequest(lens::EntryPoint::UNKNOWN);
  EXPECT_THAT(query_param, MatchesRegex("re=df&s=4&st=\\d+"));
}

TEST(LensUrlUtilsTest, Base64EncodeRequestIdTest) {
  lens::LensOverlayRequestId request_id;
  request_id.set_uuid(1);
  request_id.set_sequence_id(2);
  request_id.set_analytics_id("ABC");
  std::string encoded_id = lens::Base64EncodeRequestId(request_id);
  EXPECT_FALSE(encoded_id.empty());
  EXPECT_EQ(encoded_id, "CAEQAiIDQUJD");
}

TEST(LensUrlUtilsTest, AppendInvocationSourceParamToUrlAppendsEntryPoints) {
  constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";
  const GURL base_url(kResultsSearchBaseUrl);

  std::string expected_app_menu_url =
      base::StringPrintf("%s?source=chrome.cr.menu", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendInvocationSourceParamToURL(
                base_url, lens::LensOverlayInvocationSource::kAppMenu),
            expected_app_menu_url);

  std::string expected_context_menu_page_url =
      base::StringPrintf("%s?source=chrome.cr.ctxp", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendInvocationSourceParamToURL(
                base_url,
                lens::LensOverlayInvocationSource::kContentAreaContextMenuPage),
            expected_context_menu_page_url);

  std::string expected_context_menu_image_url =
      base::StringPrintf("%s?source=chrome.cr.ctxi", kResultsSearchBaseUrl);
  EXPECT_EQ(
      lens::AppendInvocationSourceParamToURL(
          base_url,
          lens::LensOverlayInvocationSource::kContentAreaContextMenuImage),
      expected_context_menu_image_url);

  std::string expected_toolbar_url =
      base::StringPrintf("%s?source=chrome.cr.tbic", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendInvocationSourceParamToURL(
                base_url, lens::LensOverlayInvocationSource::kToolbar),
            expected_toolbar_url);

  std::string expected_find_in_page_url =
      base::StringPrintf("%s?source=chrome.cr.find", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendInvocationSourceParamToURL(
                base_url, lens::LensOverlayInvocationSource::kFindInPage),
            expected_find_in_page_url);

  std::string expected_omnibox_url =
      base::StringPrintf("%s?source=chrome.cr.obic", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendInvocationSourceParamToURL(
                base_url, lens::LensOverlayInvocationSource::kOmnibox),
            expected_omnibox_url);

  std::string expected_context_menu_video_url =
      base::StringPrintf("%s?source=chrome.cr.ctxv", kResultsSearchBaseUrl);
  EXPECT_EQ(
      lens::AppendInvocationSourceParamToURL(
          base_url,
          lens::LensOverlayInvocationSource::kContentAreaContextMenuVideo),
      expected_context_menu_video_url);
}

}  // namespace lens
