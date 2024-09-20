// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"

#include "base/base64url.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "components/lens/lens_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/lens_overlay_knowledge_intent_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_knowledge_query.pb.h"
#include "third_party/lens_server_proto/lens_overlay_stickiness_signals.pb.h"
#include "third_party/lens_server_proto/lens_overlay_translate_stickiness_signals.pb.h"
#include "third_party/lens_server_proto/lens_overlay_video_context_input_params.pb.h"
#include "third_party/lens_server_proto/lens_overlay_video_params.pb.h"
#include "third_party/omnibox_proto/search_context.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace lens {

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";
constexpr char kResultsRedirectBaseUrl[] = "https://www.google.com/url";
constexpr char kLanguage[] = "en-US";
constexpr char kPageUrl[] = "https://www.google.com";
constexpr char kPageTitle[] = "Page Title";

class LensOverlayUrlBuilderTest : public testing::Test {
 public:
  void SetUp() override {
    g_browser_process->SetApplicationLocale(kLanguage);
    // Set all the feature params here to keep the test consistent if future
    // default values are changed.
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {
              {"results-search-url", kResultsSearchBaseUrl},
          }},
         {lens::features::kLensOverlayContextualSearchbox,
          {
              {"use-video-context-for-text-only-requests", "true"},
              {"use-video-context-for-multimodal-requests", "true"},
          }}},
        /*disabled_features=*/{});
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

  std::string EncodeVideoContext(std::optional<GURL> page_url) {
    lens::LensOverlayVideoParams video_params;
    video_params.mutable_video_context_input_params()->set_url(
        page_url->spec());
    std::string serialized_video_params;
    EXPECT_TRUE(video_params.SerializeToString(&serialized_video_params));
    std::string encoded_video_params;
    base::Base64UrlEncode(serialized_video_params,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_video_params);
    return encoded_video_params;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(LensOverlayUrlBuilderTest, AppendTranslateParamsToMap) {
  std::string query = "test";
  std::map<std::string, std::string> params;

  lens::AppendTranslateParamsToMap(params, query, "auto");

  lens::StickinessSignals expected_proto;
  expected_proto.set_id_namespace(lens::StickinessSignals::TRANSLATE_LITE);
  auto* intent_query = expected_proto.mutable_interpretation()
                           ->mutable_message_set_extension()
                           ->mutable_intent_query();
  intent_query->set_name("Translate");
  intent_query->mutable_signals()
      ->mutable_translate_stickiness_signals()
      ->set_translate_suppress_echo_for_sticky(false);
  auto* text_argument = intent_query->add_argument();
  text_argument->set_name("Text");
  text_argument->mutable_value()->mutable_simple_value()->set_string_value(
      "test");

  std::string compressed_proto;
  ASSERT_TRUE(base::Base64UrlDecode(
      params["stick"], base::Base64UrlDecodePolicy::DISALLOW_PADDING,
      &compressed_proto));
  std::string serialized_proto;
  ASSERT_TRUE(compression::GzipUncompress(compressed_proto, &serialized_proto));
  lens::StickinessSignals stickiness_signals;
  ASSERT_TRUE(stickiness_signals.ParseFromString(serialized_proto));
  EXPECT_THAT(stickiness_signals, base::test::EqualsProto(expected_proto));
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURL) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_url =
      base::StringPrintf("%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0",
                         kResultsSearchBaseUrl, text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLForLensTextSelection) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&q=%s&lns_fp=1&lns_mode=text&gsc=2&hl=%s&cs=0",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kLensTextSelection,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest,
       BuildTextOnlySearchURLWithVideoContextFlagOff) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlay,
        {
            {"results-search-url", kResultsSearchBaseUrl},
        }},
       {lens::features::kLensOverlayContextualSearchbox,
        {
            {"use-video-context-for-text-only-requests", "false"},
        }}},
      /*disabled_features=*/{});

  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_video_context =
      EncodeVideoContext(std::make_optional<GURL>(kPageUrl));

  std::string expected_url =
      base::StringPrintf("%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0",
                         kResultsSearchBaseUrl, text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle), additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWithPageUrlAndTitle) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_video_context =
      EncodeVideoContext(std::make_optional<GURL>(kPageUrl));

  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0&"
      "vidcip=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      expected_video_context.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle), additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWithPageUrl) {
  std::string text_query = "Apples";
  std::map<std::string, std::string> additional_params;
  std::string expected_video_context =
      EncodeVideoContext(std::make_optional<GURL>(kPageUrl));

  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0&"
      "vidcip=%s",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      expected_video_context.c_str());

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLEmpty) {
  std::string text_query = "";
  std::map<std::string, std::string> additional_params;
  std::string expected_url =
      base::StringPrintf("%s?source=chrome.cr.menu&q=&gsc=2&hl=%s&cs=0",
                         kResultsSearchBaseUrl, kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLPunctuation) {
  std::string text_query = "Red Apples!?#";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0", kResultsSearchBaseUrl,
      escaped_text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildTextOnlySearchURLWhitespace) {
  std::string text_query = "Red Apples";
  std::map<std::string, std::string> additional_params;
  std::string escaped_text_query =
      base::EscapeQueryParamValue(text_query, /*use_plus=*/true);
  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&q=%s&gsc=2&hl=%s&cs=0", kResultsSearchBaseUrl,
      escaped_text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::BuildTextOnlySearchURL(
                text_query,
                /*page_url=*/std::nullopt,
                /*page_title=*/std::nullopt, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                lens::TextOnlyQueryType::kSearchBoxQuery,
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
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&q=%s&lns_mode=mu&"
      "lns_fp=1&gsessionid=&udm=24&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, escaped_text_query.c_str(),
      EncodeRequestId(request_id.get()).c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          text_query, /*page_url=*/std::nullopt, /*page_title=*/std::nullopt,
          std::move(request_id), cluster_info, additional_params,
          lens::LensOverlayInvocationSource::kAppMenu,
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
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&q=%s&lns_mode=mu&"
      "lns_fp=1&gsessionid=%s&udm=24&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, escaped_text_query.c_str(),
      search_session_id.c_str(), EncodeRequestId(request_id.get()).c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          text_query, /*page_url=*/std::nullopt, /*page_title=*/std::nullopt,
          std::move(request_id), cluster_info, additional_params,
          lens::LensOverlayInvocationSource::kAppMenu,
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
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&q=&lns_mode=un&"
      "lns_fp=1&gsessionid=%s&udm=26&vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          /*text_query=*/std::nullopt, /*page_url=*/std::nullopt,
          /*page_title=*/std::nullopt, std::move(request_id), cluster_info,
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
      "%s?source=chrome.cr.menu&param=value&gsc=2&hl=%s&cs=0&q=&lns_"
      "mode=un&lns_fp=1&gsessionid=%s&udm=26&"
      "vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(
      lens::BuildLensSearchURL(
          /*text_query=*/std::nullopt, /*page_url=*/std::nullopt,
          /*page_title=*/std::nullopt, std::move(request_id), cluster_info,
          additional_params, lens::LensOverlayInvocationSource::kAppMenu,
          /*use_dark_mode=*/false),
      expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, BuildMultimodalSearchURLWithVideoContext) {
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

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_video_context =
      EncodeVideoContext(std::make_optional<GURL>(kPageUrl));
  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&vidcip=%s&q=%s&lns_"
      "mode=mu&lns_fp=1&gsessionid=%s&udm=24&"
      "vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, expected_video_context.c_str(),
      escaped_text_query.c_str(), search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle),
                std::move(request_id), cluster_info, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}
TEST_F(LensOverlayUrlBuilderTest,
       BuildImageOnlySearchURLWithVideoContextDoesNotAttachContext) {
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
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&q=&lns_"
      "mode=un&lns_fp=1&gsessionid=%s&udm=26&"
      "vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, search_session_id.c_str(),
      encoded_request_id.c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(
                /*text_query=*/std::nullopt, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle),
                std::move(request_id), cluster_info, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest,
       BuildMultimodalSearchURLWithVideoContextFlagOff) {
  feature_list_.Reset();
  feature_list_.InitWithFeaturesAndParameters(
      {{lens::features::kLensOverlay,
        {
            {"results-search-url", kResultsSearchBaseUrl},
        }},
       {lens::features::kLensOverlayContextualSearchbox,
        {
            {"use-video-context-for-multimodal-requests", "false"},
        }}},
      /*disabled_features=*/{});

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

  std::string serialized_request_id;
  CHECK(request_id.get()->SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::string expected_url = base::StringPrintf(
      "%s?source=chrome.cr.menu&gsc=2&hl=%s&cs=0&q=%s&lns_"
      "mode=mu&lns_fp=1&gsessionid=%s&udm=24&"
      "vsrid=%s",
      kResultsSearchBaseUrl, kLanguage, escaped_text_query.c_str(),
      search_session_id.c_str(), encoded_request_id.c_str());

  EXPECT_EQ(lens::BuildLensSearchURL(
                text_query, std::make_optional<GURL>(kPageUrl),
                std::make_optional<std::string>(kPageTitle),
                std::move(request_id), cluster_info, additional_params,
                lens::LensOverlayInvocationSource::kAppMenu,
                /*use_dark_mode=*/false),
            expected_url);
}

TEST_F(LensOverlayUrlBuilderTest, HasCommonSearchQueryParameters) {
  const GURL url(base::StringPrintf("%s?gsc=2&hl=%s&cs=1",
                                    kResultsSearchBaseUrl, kLanguage));
  EXPECT_TRUE(lens::HasCommonSearchQueryParameters(url));
}

TEST_F(LensOverlayUrlBuilderTest, HasCommonSearchQueryParametersWithoutLocale) {
  const GURL url(base::StringPrintf("%s?gsc=2", kResultsSearchBaseUrl));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(url));
}

TEST_F(LensOverlayUrlBuilderTest,
       HasCommonSearchQueryParametersMissingQueryParams) {
  const GURL failing_url1(
      base::StringPrintf("%s?gsc=2", kResultsSearchBaseUrl));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(failing_url1));

  const GURL failing_url2(
      base::StringPrintf("%s?hl=%s", kResultsSearchBaseUrl, kLanguage));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(failing_url2));

  const GURL failing_url3(base::StringPrintf("%s?cs=1", kResultsSearchBaseUrl));
  EXPECT_FALSE(lens::HasCommonSearchQueryParameters(failing_url3));
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

TEST_F(LensOverlayUrlBuilderTest, RemoveIgnoredSearchURLParameters) {
  std::string text_query = "Apples";
  std::string viewport_width = "400";
  std::string viewport_height = "500";
  std::string initial_url = base::StringPrintf(
      "%s?q=%s&gsc=2&hl=%s&biw=%s&bih=%s&sec_act=1&sxsrf=token",
      kResultsSearchBaseUrl, text_query.c_str(), kLanguage,
      viewport_width.c_str(), viewport_height.c_str());
  std::string expected_url =
      base::StringPrintf("%s?q=%s&gsc=2&hl=%s", kResultsSearchBaseUrl,
                         text_query.c_str(), kLanguage);

  EXPECT_EQ(lens::RemoveIgnoredSearchURLParameters(GURL(initial_url)),
            GURL(expected_url));
}

TEST_F(LensOverlayUrlBuilderTest, GetSearchResultsUrlFromRedirectUrl) {
  std::string text_query = "Apples";
  std::string viewport_width = "400";
  std::string viewport_height = "500";
  std::string relative_search_url = base::StringPrintf(
      "/search?q=%s&gsc=2&hl=%s&biw=%s&bih=%s", text_query.c_str(), kLanguage,
      viewport_width.c_str(), viewport_height.c_str());
  std::string escaped_relative_search_url =
      base::EscapeUrlEncodedData(relative_search_url, /*use_plus=*/false);
  GURL search_url = GURL(kResultsSearchBaseUrl).Resolve(relative_search_url);

  std::string initial_url =
      base::StringPrintf("%s?url=%s", kResultsRedirectBaseUrl,
                         escaped_relative_search_url.c_str());
  EXPECT_EQ(lens::GetSearchResultsUrlFromRedirectUrl(GURL(initial_url)),
            GURL(search_url));
}

}  // namespace lens
