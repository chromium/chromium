// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/search_context.pb.h"

namespace lens {

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";
constexpr char kLanguage[] = "en-US";
constexpr char kPageUrl[] = "https://www.google.com";
constexpr char kPageTitle[] = "Page Title";

class LensOverlayUrlBuilderTest : public testing::Test {
 public:
  void SetUp() override {
    g_browser_process->SetApplicationLocale(kLanguage);
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay,
        {
            {"results-search-url", kResultsSearchBaseUrl},
            {"use-search-context-for-text-only-requests", "true"},
        });
  }

  std::string EncodeRequestId(lens::LensOverlayRequestId* request_id) {
    std::string serialized_request_id;
    EXPECT_TRUE(request_id->SerializeToString(&serialized_request_id));
    std::string encoded_request_id;
    base::Base64UrlEncode(serialized_request_id,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_request_id);
    return encoded_request_id;
  }

  std::string EncodeSearchContext(std::optional<GURL> page_url,
                                  std::optional<std::string> page_title) {
    omnibox::SearchContext search_context;
    if (page_url.has_value()) {
      search_context.set_webpage_url(page_url->spec());
    }
    if (page_title.has_value()) {
      search_context.set_webpage_title(*page_title);
    }
    std::string serialized_search_context;
    EXPECT_TRUE(search_context.SerializeToString(&serialized_search_context));
    std::string encoded_search_context;
    base::Base64UrlEncode(serialized_search_context,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_search_context);
    return encoded_search_context;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURL) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest,
       BuildTextOnlySearchURLWithSearchContextFlagOff) {
  feature_list_.Reset();
  feature_list_.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlay,
      {
          {"results-search-url", kResultsSearchBaseUrl},
          {"use-search-context-for-text-only-requests", "false"},
      });

  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_search_context =
      EncodeSearchContext(std::make_optional<GURL>(kPageUrl),
                          std::make_optional<std::string>(kPageTitle));

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle), additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWithPageUrlAndTitle) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_search_context =
      EncodeSearchContext(std::make_optional<GURL>(kPageUrl),
                          std::make_optional<std::string>(kPageTitle));

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s&masfc=c&"
      "mactx=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      expected_search_context.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle), additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWithPageUrl) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_search_context = EncodeSearchContext(
      std::make_optional<GURL>(kPageUrl), /*page_title=*/std::nullopt);

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s&masfc=c&"
      "mactx=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      expected_search_context.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWithPageTitle) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_search_context = EncodeSearchContext(
      /*page_url=*/std::nullopt, std::make_optional<std::string>(kPageTitle));

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s&masfc=c&"
      "mactx=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      expected_search_context.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                std::make_optional<std::string>(kPageTitle), additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLEmpty) {
  std::string text_query = "";
  std::map<std::string, std::string> additional_params;
  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=&lns_mode=text&gsc=1&hl=%s",
      kResultsSearchBaseUrl, kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLPunctuation) {
  std::string text_query = "Red Apples!?#";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s",
      kResultsSearchBaseUrl, escaped_text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWhitespace) {
  std::string text_query = "Red Apples";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&q=%s&lns_mode=text&gsc=1&hl=%s",
      kResultsSearchBaseUrl, escaped_text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLEmptyClusterInfo) {
  std::string text_query = "Green Apples";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;

  lens::LensOverlayClusterInfo cluster_info;
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&gsc=1&hl=%s&q=%s&lns_mode=mu&"
      "gsessionid=&udm=24&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, escaped_text_query.c_str(),
      EncodeRequestId(request_id.get()).c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(
                text_query, std::move(request_id), cluster_info,
                additional_params, lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLWithSessionId) {
  std::string text_query = "Green Apples";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;
  std::string search_session_id = "search_session_id";

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  cluster_info.set_search_session_id(search_session_id);
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&gsc=1&hl=%s&q=%s&lns_mode=mu&"
      "gsessionid=%s&udm=24&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, escaped_text_query.c_str(),
      search_session_id.c_str(), EncodeRequestId(request_id.get()).c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(
                text_query, std::move(request_id), cluster_info,
                additional_params, lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLWithNoTextQuery) {
  std::map<std::string, std::string> additional_params;
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;
  std::string search_session_id = "search_session_id";

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  cluster_info.set_search_session_id(search_session_id);
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&gsc=1&hl=%s&q=&lns_mode=un&"
      "gsessionid=%s&udm=26&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          /*text_query=*/std::nullopt, std::move(request_id), cluster_info,
          additional_params, lens::LensOverlayInvocationSource::kAppMenu,
          /*use_dark_mode=*/false),
      expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildLensSearchURLWithAdditionalParams) {
  std::map<std::string, std::string> additional_params = {{"param", "value"}};
  uint64_t uuid = 12345;
  int sequence_id = 1;
  int image_sequence_id = 3;
  std::string search_session_id = "search_session_id";

  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      std::make_unique<lens::LensOverlayRequestId>();
  lens::LensOverlayClusterInfo cluster_info;
  cluster_info.set_search_session_id(search_session_id);
  request_id->set_uuid(uuid);
  request_id->set_sequence_id(sequence_id);
  request_id->set_image_sequence_id(image_sequence_id);

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url = base::StringPrintf(
      "%s?cs=0&source=chrome.cr.menu&param=value&gsc=1&hl=%s&q=&lns_"
      "mode=un&gsessionid=%s&udm=26&"
      "vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          /*text_query=*/std::nullopt, std::move(request_id), cluster_info,
          additional_params, lens::LensOverlayInvocationSource::kAppMenu,
          /*use_dark_mode=*/false),
      expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, HasCommonSearchQueryParameters) {
  const GURL url(
      base::StringPrintf("%s?gsc=1&hl=%s", kResultsSearchBaseUrl, kLanguage));
  EXPECT_TRUE(lens::HasCommonSearchQueryParameters(url));
}

TEST_F(LensOverlayUrlBuilderTest, HasCommonSearchQueryParametersWithoutLocale) {
  const GURL url(base::StringPrintf("%s?gsc=1", kResultsSearchBaseUrl));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(url));
}

TEST_F(LensOverlayUrlBuilderTest,
       HasCommonSearchQueryParametersMissingQueryParams) {
  const GURL failing_url1(
      base::StringPrintf("%s?gsc=1", kResultsSearchBaseUrl));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(failing_url1));

  const GURL failing_url2(
      base::StringPrintf("%s?hl=%s", kResultsSearchBaseUrl, kLanguage));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(failing_url2));
}

TEST_F(LensOverlayUrlBuilderTest,
       AppendInvocationSourceParamToUrlAppendsEntryPoints) {
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
}

TEST_F(LensOverlayUrlBuilderTest,
       AppendDarkModeParamToURLToUrlAppendsDarkMode) {
  const GURL base_url(kResultsSearchBaseUrl);

  std::string expected_light_mode_url =
      base::StringPrintf("%s?cs=0", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendDarkModeParamToURL(base_url, /*use_dark_mode=*/false),
            expected_light_mode_url);

  std::string expected_dark_mode_url =
      base::StringPrintf("%s?cs=1", kResultsSearchBaseUrl);
  EXPECT_EQ(lens::AppendDarkModeParamToURL(base_url, /*use_dark_mode=*/true),
            expected_dark_mode_url);
}

TEST_F(LensOverlayUrlBuilderTest, HasCommonSearchQueryParametersNoQueryParams) {
  const GURL url(kResultsSearchBaseUrl);
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(url));
}

TEST_F(LensOverlayUrlBuilderTest, IsValidSearchURL) {
  EXPECT_TRUE(lens::IsValidSearchResultsUrl(GURL(kResultsSearchBaseUrl)));
}

TEST_F(LensOverlayUrlBuilderTest, IsValidSearchURLDifferentDomains) {
  EXPECT_FALSE(
      lens::IsValidSearchResultsUrl(GURL("https://test.google/search")));
}

TEST_F(LensOverlayUrlBuilderTest, IsValidSearchURLDifferentSchemes) {
  // GetContent() should return everything after the scheme.
  GURL different_scheme_url = GURL(base::StringPrintf(
      "chrome://%s", GURL(kResultsSearchBaseUrl).GetContent().c_str()));
  EXPECT_FALSE(lens::IsValidSearchResultsUrl(different_scheme_url));
}

TEST_F(LensOverlayUrlBuilderTest, IsValidSearchURLDifferentPaths) {
  EXPECT_FALSE(lens::IsValidSearchResultsUrl(
      GURL(kResultsSearchBaseUrl).GetWithEmptyPath()));
}

TEST_F(LensOverlayUrlBuilderTest, IsValidSearchURLInvalidURL) {
  EXPECT_FALSE(lens::IsValidSearchResultsUrl(GURL()));
}

TEST_F(LensOverlayUrlBuilderTest, RemoveViewportParamFromURL) {
  std::string text_query = "Apples";
  std::string viewport_width = "400";
  std::string viewport_height = "500";
  std::string initial_url =
      base::StringPrintf("%s?q=%s&gsc=1&hl=%s&biw=%s&bih=%s",
                         kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
                         viewport_width.c_str(), viewport_height.c_str());
  std::string expected_url =
      base::StringPrintf("%s?q=%s&gsc=1&hl=%s", kResultsSearchBaseUrl,
                         text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::RemoveUrlViewportParams(GURL(initial_url)),
            GURL(expected_url));
}

}  // namespace lens
