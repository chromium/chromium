// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/search_engines/template_url.h"

#include <stddef.h>

#include <string>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/regulatory_extension_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/omnibox_proto/chrome_searchbox_stats.pb.h"
#include "ui/base/device_form_factor.h"

using base::ASCIIToUTF16;
using CreatedByPolicy = TemplateURLData::CreatedByPolicy;
using RequestSource = SearchTermsData::RequestSource;

namespace {
bool IsLowerCase(const std::u16string& str) {
  return str == base::i18n::ToLower(str);
}
}

class TemplateURLTest : public testing::Test {
 public:
  TemplateURLTest() : search_terms_data_("http://www.google.com/") {}
  void CheckSuggestBaseURL(const std::string& base_url,
                           const std::string& base_suggest_url) const;

  static void ExpectPostParamIs(
      const TemplateURLRef::PostParam& param,
      const std::string& name,
      const std::string& value,
      const std::string& content_type = std::string());

  static void ExpectContainsPostParam(
      const TemplateURLRef::PostParams& params,
      const std::string& name,
      const std::string& value,
      const std::string& content_type = std::string());

 protected:
  void TestReplaceSearcboxStats(bool is_prefetch,
                                const std::string& prefetch_param,
                                const std::string& gs_lcrp_param);

  TestingSearchTermsData search_terms_data_;
};

void TemplateURLTest::CheckSuggestBaseURL(
    const std::string& base_url,
    const std::string& base_suggest_url) const {
  TestingSearchTermsData search_terms_data(base_url);
  EXPECT_EQ(base_suggest_url, search_terms_data.GoogleBaseSuggestURLValue());
}

// static
void TemplateURLTest::ExpectPostParamIs(const TemplateURLRef::PostParam& param,
                                        const std::string& name,
                                        const std::string& value,
                                        const std::string& content_type) {
  EXPECT_EQ(name, param.name);
  EXPECT_EQ(value, param.value);
  EXPECT_EQ(content_type, param.content_type);
}

// static
void TemplateURLTest::ExpectContainsPostParam(
    const TemplateURLRef::PostParams& params,
    const std::string& name,
    const std::string& value,
    const std::string& content_type) {
  for (const auto& param : params) {
    if (param.name == name && param.value == value &&
        param.content_type == content_type) {
      return;
    }
  }
  FAIL() << "Expected post param not found.";
}

TEST_F(TemplateURLTest, Defaults) {
  TemplateURLData data;
  EXPECT_FALSE(data.safe_for_autoreplace);
  EXPECT_EQ(0, data.prepopulate_id);
}

TEST_F(TemplateURLTest, TestValidWithComplete) {
  TemplateURLData data;
  data.SetURL("{searchTerms}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
}

TEST_F(TemplateURLTest, URLRefTestSearchTerms) {
  struct SearchTermsCase {
    const char* url;
    const std::u16string terms;
    const std::string output;
  } search_term_cases[] = {
      {"http://foo{searchTerms}", u"sea rch/bar", "http://foosea%20rch/bar"},
      {"http://foo{searchTerms}?boo=abc", u"sea rch/bar",
       "http://foosea%20rch/bar?boo=abc"},
      {"http://foo/?boo={searchTerms}", u"sea rch/bar",
       "http://foo/?boo=sea+rch%2Fbar"},
      {"http://en.wikipedia.org/{searchTerms}", u"wiki/?",
       "http://en.wikipedia.org/wiki/%3F"}};
  for (size_t i = 0; i < std::size(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    TemplateURLData data;
    data.SetURL(value.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(value.terms), search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTestCount) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}{count?}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestCount2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}{count}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foox10/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{startIndex?}y{startPage?}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxy/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestIndices2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{startIndex}y{startPage}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxx1y1/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{inputEncoding?}y{outputEncoding?}a");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8ya/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestImageURLWithPOST) {
  // List all accpectable parameter format in valid_post_params_string. it is
  // expected like: "name0=,name1=value1,name2={template1}"
  const char kValidPostParamsString[] =
      "image_content={google:imageThumbnail},image_url={google:imageURL},"
      "sbisrc={google:imageSearchSource},language={language},empty_param=,"
      "constant_param=constant,width={google:imageOriginalWidth},"
      "base64_image_content={google:imageThumbnailBase64}";
  const char KImageSearchURL[] = "http://foo.com/sbi";

  TemplateURLData data;
  data.image_url = KImageSearchURL;

  // Try to parse invalid post parameters.
#if !BUILDFLAG(IS_ANDROID)
  const char kInvalidPostParamsString[] =
      "unknown_template={UnknownTemplate},bad_value=bad{value},"
      "{google:sbiSource}";
  data.image_url_post_params = kInvalidPostParamsString;
  TemplateURL url_bad(data);
  ASSERT_FALSE(url_bad.image_url_ref().IsValid(search_terms_data_));
  const TemplateURLRef::PostParams& bad_post_params =
      url_bad.image_url_ref().post_params_;
  ASSERT_EQ(2U, bad_post_params.size());
  ExpectPostParamIs(bad_post_params[0], "unknown_template", "");
  ExpectPostParamIs(bad_post_params[1], "bad_value", "bad{value}");
#endif

  // Try to parse valid post parameters.
  data.image_url_post_params = kValidPostParamsString;
  TemplateURL url(data);
  ASSERT_TRUE(url.image_url_ref().IsValid(search_terms_data_));
  ASSERT_FALSE(url.image_url_ref().SupportsReplacement(search_terms_data_));

  // Check term replacement.
  TemplateURLRef::SearchTermsArgs search_args(u"X");
  search_args.image_thumbnail_content = "dummy-image-thumbnail";
  search_args.image_url = GURL("http://dummyimage.com/dummy.jpg");
  search_args.image_original_size = gfx::Size(10, 10);
  // Replacement operation with no post_data buffer should still return
  // the parsed URL.
  TestingSearchTermsData search_terms_data("http://X");
  GURL result(url.image_url_ref().ReplaceSearchTerms(
      search_args, search_terms_data));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ(KImageSearchURL, result.spec());
  TemplateURLRef::PostContent post_content;
  result = GURL(url.image_url_ref().ReplaceSearchTerms(
      search_args, search_terms_data, &post_content));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ(KImageSearchURL, result.spec());
  ASSERT_FALSE(post_content.first.empty());
  ASSERT_FALSE(post_content.second.empty());

  // Check parsed result of post parameters.
  const TemplateURLRef::Replacements& replacements =
      url.image_url_ref().replacements_;
  const TemplateURLRef::PostParams& post_params =
      url.image_url_ref().post_params_;
  EXPECT_EQ(8U, post_params.size());
  for (auto i = post_params.begin(); i != post_params.end(); ++i) {
    auto j = replacements.begin();
    for (; j != replacements.end(); ++j) {
      if (j->is_post_param && j->index ==
          static_cast<size_t>(i - post_params.begin())) {
        switch (j->type) {
          case TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_WIDTH:
            ExpectPostParamIs(
                *i, "width",
                base::NumberToString(search_args.image_original_size.width()));
            break;
          case TemplateURLRef::GOOGLE_IMAGE_SEARCH_SOURCE:
            ExpectPostParamIs(*i, "sbisrc",
                              search_terms_data.GoogleImageSearchSource());
            break;
          case TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL:
            ExpectPostParamIs(*i, "image_content",
                              search_args.image_thumbnail_content,
                              "image/jpeg");
            break;
          case TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL_BASE64: {
            std::string base64_image_content =
                base::Base64Encode(search_args.image_thumbnail_content);
            ExpectPostParamIs(*i, "base64_image_content", base64_image_content,
                              "image/jpeg");
            break;
          }
          case TemplateURLRef::GOOGLE_IMAGE_URL:
            ExpectPostParamIs(*i, "image_url", search_args.image_url.spec());
            break;
          case TemplateURLRef::LANGUAGE:
            ExpectPostParamIs(*i, "language", "en");
            break;
          default:
            ADD_FAILURE();  // Should never go here.
        }
        break;
      }
    }
    if (j != replacements.end())
      continue;
    if (i->name == "empty_param")
      ExpectPostParamIs(*i, "empty_param", std::string());
    else
      ExpectPostParamIs(*i, "constant_param", "constant");
  }
}

TEST_F(TemplateURLTest, ImageThumbnailContentTypePostParams) {
  TemplateURLData data;
  data.image_url = "http://foo.com/sbi";
  data.image_url_post_params =
      "image_content={google:imageThumbnail},"
      "base64_image_content={google:imageThumbnailBase64}";
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));

  TemplateURLRef::SearchTermsArgs search_args(u"X");
  search_args.image_thumbnail_content = "dummy-image-thumbnail";
  search_args.image_thumbnail_content_type = "image/tiff";
  TestingSearchTermsData search_terms_data("http://X");
  GURL result(
      url.image_url_ref().ReplaceSearchTerms(search_args, search_terms_data));
  ASSERT_TRUE(result.is_valid());

  const TemplateURLRef::PostParams& post_params =
      url.image_url_ref().post_params_;
  ExpectContainsPostParam(post_params, "image_content",
                          search_args.image_thumbnail_content, "image/tiff");
  std::string base64_image_content =
      base::Base64Encode(search_args.image_thumbnail_content);
  ExpectContainsPostParam(post_params, "base64_image_content",
                          base64_image_content, "image/tiff");
}

TEST_F(TemplateURLTest, ImageURLWithGetShouldNotCrash) {
  TemplateURLData data;
  data.SetURL("http://foo/?q={searchTerms}&t={google:imageThumbnail}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));

  TemplateURLRef::SearchTermsArgs search_args(u"X");
  search_args.image_thumbnail_content = "dummy-image-thumbnail";
  GURL result(
      url.url_ref().ReplaceSearchTerms(search_args, search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://foo/?q=X&t=dummy-image-thumbnail", result.spec());
}

TEST_F(TemplateURLTest, ParsePlayStoreDefinitions) {
  // *** *** *** *** WARNING *** *** *** ***
  //
  // Do not remove elements from the list below.
  //
  // This test validates that the TemplateURL definitions served by Play Store
  // can be processed and understood by Chrome.
  //
  // The list of parameters listed below reflects parameters found in Play Store
  // configuration file. The only valid reason for modifying this list is if the
  // Play Store configuration was updated to include *new* parameters.
  //
  // As long as the PlayStore TemplateURL definitions shadow internal templates
  // this list should never drop elements.
  //
  // *** *** *** *** WARNING *** *** *** ***
  //
  // Extracted from search_engine_chrome_metadata using:
  // sed -n 's/[^{]*\({[^}]*}\)[^{]*/"\1",\n/gp' | sort | uniq
  // LINT.IfChange
  std::set<std::string> recognized_params{{
      "{google:RLZ}",
      "{google:assistedQueryStats}",
      "{google:baseSearchByImageURL}",
      "{google:baseSuggestURL}",
      "{google:baseURL}",
      "{google:contextualSearchContextData}",
      "{google:contextualSearchVersion}",
      "{google:currentPageUrl}",
      "{google:cursorPosition}",
      "{google:iOSSearchLanguage}",
      "{google:imageOriginalHeight}",
      "{google:imageOriginalWidth}",
      "{google:imageSearchSource}",
      "{google:imageThumbnailBase64}",
      "{google:imageThumbnail}",
      "{google:imageURL}",
      "{google:inputType}",
      "{google:omniboxFocusType}",
      "{google:originalQueryForSuggestion}",
      "{google:pageClassification}",
      "{google:pathWildcard}",
      "{google:prefetchQuery}",
      "{google:processedImageDimensions}",
      "{google:searchClient}",
      "{google:searchFieldtrialParameter}",
      "{google:searchVersion}",
      "{google:sessionToken}",
      "{google:sourceId}",
      "{google:suggestAPIKeyParameter}",
      "{google:suggestClient}",
      "{google:suggestRid}",
      "{imageTranslateSourceLocale}",
      "{imageTranslateTargetLocale}",
      "{inputEncoding}",
      "{language}",
      "{searchTerms}",
      "{yandex:searchPath}",
  }};
  // LINT.ThenChange(//googledata/experiments/play/features/gateway/http/setupandupdate/tests/chrome_template_url_parameters_validations.gcl)

  // Basic confirmation check; if this fails, it's possible the logic below
  // needs updating.
  EXPECT_EQ(0, SEARCH_ENGINE_OTHER);
  EXPECT_NE(0, SEARCH_ENGINE_GOOGLE);

  for (const auto& reference : recognized_params) {
    TemplateURLData data;
    data.SetURL(reference);
    TemplateURL url(data);
    TemplateURLRef::Replacements replacements;

    {
      // External:
      data.prepopulate_id = SEARCH_ENGINE_OTHER;
      std::string result = reference;

      EXPECT_TRUE(url.url_ref().ParseParameter(0, reference.length() - 1,
                                               &result, &replacements));

      // Verifying `replacements` is moot: if substitution is not available,
      // replacement won't be made. Instead, verify if resulting URL has been
      // modified.
      EXPECT_NE(result, reference);
    }

    {
      // Internal:
      data.prepopulate_id = SEARCH_ENGINE_GOOGLE;
      std::string result = reference;

      EXPECT_TRUE(url.url_ref().ParseParameter(0, reference.length() - 1,
                                               &result, &replacements));

      // Verifying `replacements` is moot: if substitution is not available,
      // replacement won't be made. Instead, verify if resulting URL has been
      // modified.
      EXPECT_NE(result, reference);
    }
  }
}

// Test that setting the prepopulate ID from TemplateURL causes the stored
// TemplateURLRef to handle parsing the URL parameters differently.
TEST_F(TemplateURLTest, SetPrepopulatedAndParse) {
  TemplateURLData data;
  data.SetURL("http://foo{fhqwhgads}bar");
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;

  EXPECT_EQ("http://foobar",
            url.url_ref().ParseURL("http://foo{fhqwhgads}bar", &replacements,
                                   nullptr, &valid));
  EXPECT_TRUE(valid);

  EXPECT_TRUE(replacements.empty());

  data.prepopulate_id = 123;
  TemplateURL url2(data);
  url2.url_ref().ParseURL("http://foo{fhqwhgads}bar", &replacements, nullptr,
                          &valid);
  EXPECT_TRUE(replacements.empty());
}

// Test that setting the prepopulate ID from TemplateURL causes the stored
// TemplateURLRef to handle parsing the URL parameters differently.
TEST_F(TemplateURLTest, SetPrepopulatedAndReplace) {
  TemplateURLData data;
  data.SetURL("http://foo{fhqwhgads}search/?q={searchTerms}");
  data.suggestions_url = "http://foo{fhqwhgads}suggest/?q={searchTerms}";
  data.image_url = "http://foo{fhqwhgads}image/";
  data.image_translate_url = "http://foo{fhqwhgads}image/?translate";
  data.new_tab_url = "http://foo{fhqwhgads}newtab/";
  data.contextual_search_url = "http://foo{fhqwhgads}context/";
  data.alternate_urls.push_back(
      "http://foo{fhqwhgads}alternate/?q={searchTerms}");

  TemplateURLRef::SearchTermsArgs args(u"X");
  const SearchTermsData& stdata = search_terms_data_;

  TemplateURL url(data);
  EXPECT_EQ("http://foosearch/?q=X",
            url.url_ref().ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://fooalternate/?q=X",
            url.url_refs()[0].ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://foosearch/?q=X",
            url.url_refs()[1].ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://foosuggest/?q=X",
            url.suggestions_url_ref().ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://fooimage/",
            url.image_url_ref().ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://fooimage/?translate",
            url.image_translate_url_ref().ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://foonewtab/",
            url.new_tab_url_ref().ReplaceSearchTerms(args, stdata));
  EXPECT_EQ("http://foocontext/",
            url.contextual_search_url_ref().ReplaceSearchTerms(args, stdata));

  data.prepopulate_id = 123;
  TemplateURL url2(data);
  url2.url_ref().ReplaceSearchTerms(args, stdata);
}

TEST_F(TemplateURLTest, InputEncodingBeforeSearchTerm) {
  TemplateURLData data;
  data.SetURL("http://foox{inputEncoding?}a{searchTerms}y{outputEncoding?}b");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxutf-8axyb/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestEncoding2) {
  TemplateURLData data;
  data.SetURL("http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://fooxxutf-8yutf-8a/", result.spec());
}

TEST_F(TemplateURLTest, URLRefTestSearchTermsUsingTermsData) {
  struct SearchTermsCase {
    const char* url;
    const std::u16string terms;
    const char* output;
  } search_term_cases[] = {{"{google:baseURL}{language}{searchTerms}",
                            std::u16string(), "http://example.com/e/en"},
                           {"{google:baseSuggestURL}{searchTerms}",
                            std::u16string(), "http://example.com/complete/"}};

  TestingSearchTermsData search_terms_data("http://example.com/e/");
  TemplateURLData data;
  for (size_t i = 0; i < std::size(search_term_cases); ++i) {
    const SearchTermsCase& value = search_term_cases[i];
    data.SetURL(value.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(value.terms), search_terms_data,
        nullptr));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(value.output, result.spec());
  }
}

TEST_F(TemplateURLTest, URLRefTermToWide) {
  struct ToWideCase {
    const char* encoded_search_term;
    const std::u16string expected_decoded_term;
  } to_wide_cases[] = {
      {"hello+world", u"hello world"},
      // Test some big-5 input.
      {"%a7A%A6%6e+to+you", u"\x4f60\x597d to you"},
      // Test some UTF-8 input. We should fall back to this when the encoding
      // doesn't look like big-5. We have a '5' in the middle, which is an
      // invalid Big-5 trailing byte.
      {"%e4%bd%a05%e5%a5%bd+to+you", u"\x4f60\x35\x597d to you"},
      // Undecodable input should stay escaped.
      {"%91%01+abcd", u"%91%01 abcd"},
      // Make sure we convert %2B to +.
      {"C%2B%2B", u"C++"},
      // C%2B is escaped as C%252B, make sure we unescape it properly.
      {"C%252B", u"C%2B"},
  };

  // Set one input encoding: big-5. This is so we can test fallback to UTF-8.
  TemplateURLData data;
  data.SetURL("http://foo?q={searchTerms}");
  data.input_encodings.push_back("big-5");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  for (size_t i = 0; i < std::size(to_wide_cases); i++) {
    EXPECT_EQ(to_wide_cases[i].expected_decoded_term,
              url.url_ref().SearchTermToString16(
                  to_wide_cases[i].encoded_search_term));
  }
}

TEST_F(TemplateURLTest, DisplayURLToURLRef) {
  struct TestData {
    const std::string url;
    const std::u16string expected_result;
  } test_data[] = {
      {"http://foo{searchTerms}x{inputEncoding}y{outputEncoding}a",
       u"http://foo%sx{inputEncoding}y{outputEncoding}a"},
      {"http://X", u"http://X"},
      {"http://foo{searchTerms", u"http://foo{searchTerms"},
      {"http://foo{searchTerms}{language}", u"http://foo%s{language}"},
  };
  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_EQ(entry.expected_result,
              url.url_ref().DisplayURL(search_terms_data_));
    EXPECT_EQ(entry.url, TemplateURLRef::DisplayURLToURLRef(
                             url.url_ref().DisplayURL(search_terms_data_)));
  }
}

TEST_F(TemplateURLTest, ReplaceSearchTerms) {
  struct TestData {
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
    { "http://foo/{language}{searchTerms}{inputEncoding}",
      "http://foo/{language}XUTF-8" },
    { "http://foo/{language}{inputEncoding}{searchTerms}",
      "http://foo/{language}UTF-8X" },
    { "http://foo/{searchTerms}{language}{inputEncoding}",
      "http://foo/X{language}UTF-8" },
    { "http://foo/{searchTerms}{inputEncoding}{language}",
      "http://foo/XUTF-8{language}" },
    { "http://foo/{inputEncoding}{searchTerms}{language}",
      "http://foo/UTF-8X{language}" },
    { "http://foo/{inputEncoding}{language}{searchTerms}",
      "http://foo/UTF-8{language}X" },
    { "http://foo/{language}a{searchTerms}a{inputEncoding}a",
      "http://foo/{language}aXaUTF-8a" },
    { "http://foo/{language}a{inputEncoding}a{searchTerms}a",
      "http://foo/{language}aUTF-8aXa" },
    { "http://foo/{searchTerms}a{language}a{inputEncoding}a",
      "http://foo/Xa{language}aUTF-8a" },
    { "http://foo/{searchTerms}a{inputEncoding}a{language}a",
      "http://foo/XaUTF-8a{language}a" },
    { "http://foo/{inputEncoding}a{searchTerms}a{language}a",
      "http://foo/UTF-8aXa{language}a" },
    { "http://foo/{inputEncoding}a{language}a{searchTerms}a",
      "http://foo/UTF-8a{language}aXa" },
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    std::string expected_result = entry.expected_result;
    base::ReplaceSubstringsAfterOffset(
        &expected_result, 0, "{language}",
        search_terms_data_.GetApplicationLocale());
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(u"X"), search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(expected_result, result.spec());
  }
}


// Tests replacing search terms in various encodings and making sure the
// generated URL matches the expected value.
TEST_F(TemplateURLTest, ReplaceArbitrarySearchTerms) {
  struct TestData {
    const std::string encoding;
    const std::u16string search_term;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      {"BIG5", u"悽", "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%B1~BIG5"},
      {"UTF-8", u"blah", "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?blahUTF-8"},
      {"Shift_JIS", u"あ", "http://foo/{searchTerms}/bar",
       "http://foo/%82%A0/bar"},
      {"Shift_JIS", u"あ い", "http://foo/{searchTerms}/bar",
       "http://foo/%82%A0%20%82%A2/bar"},
  };
  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    data.input_encodings.clear();
    data.input_encodings.push_back(entry.encoding);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(entry.search_term),
        search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

// Test that encoding with several optional codepages works as intended.
// Codepages are tried in order, fallback is UTF-8.
TEST_F(TemplateURLTest, ReplaceSearchTermsMultipleEncodings) {
  struct TestData {
    const std::vector<std::string> encodings;
    const std::u16string search_term;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      // First and third encodings are valid. First is used.
      {{"windows-1251", "cp-866", "UTF-8"},
       u"я",
       "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%FFwindows-1251"},
      // Second and third encodings are valid, second is used.
      {{"cp-866", "GB2312", "UTF-8"},
       u"狗",
       "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%B9%B7GB2312"},
      // Second and third encodings are valid in another order, second is used.
      {{"cp-866", "UTF-8", "GB2312"},
       u"狗",
       "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%E7%8B%97UTF-8"},
      // Both encodings are invalid, fallback to UTF-8.
      {{"cp-866", "windows-1251"},
       u"狗",
       "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%E7%8B%97UTF-8"},
      // No encodings are given, fallback to UTF-8.
      {{},
       u"狗",
       "http://foo/?{searchTerms}{inputEncoding}",
       "http://foo/?%E7%8B%97UTF-8"},
  };

  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    data.input_encodings = entry.encodings;
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(entry.search_term),
        search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

// Tests replacing prefetch parameters (pf) and searchbox stats (gs_lcrp) in
// various scenarios.
void TemplateURLTest::TestReplaceSearcboxStats(
    bool is_prefetch,
    const std::string& expected_prefetch_param,
    const std::string& expected_gs_lcrp_param) {
  omnibox::metrics::ChromeSearchboxStats searchbox_stats;
  searchbox_stats.set_client_name("chrome");
  searchbox_stats.set_zero_prefix_enabled(true);

  struct TestData {
    const std::u16string search_term;
    const omnibox::metrics::ChromeSearchboxStats searchbox_stats;
    const std::string base_url;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      // HTTPS and non-empty gs_lcrp: Success.
      {u"foo", searchbox_stats, "https://foo/",
       "{google:baseURL}?q={searchTerms}&{google:assistedQueryStats}",
       "https://foo/?q=foo&" + expected_gs_lcrp_param +
           expected_prefetch_param},
      // Non-Google HTTPS and non-empty gs_lcrp: Success.
      {u"foo", searchbox_stats, "https://bar/",
       "https://foo/?q={searchTerms}&{google:assistedQueryStats}",
       "https://foo/?q=foo&" + expected_gs_lcrp_param +
           expected_prefetch_param},
      // No HTTPS: Failure.
      {u"foo", searchbox_stats, "http://foo/",
       "{google:baseURL}?q={searchTerms}&{google:assistedQueryStats}",
       "http://foo/?q=foo&" + expected_prefetch_param},
      // No {google:assistedQueryStats}: Failure.
      {u"foo", searchbox_stats, "https://foo/",
       "{google:baseURL}?q={searchTerms}", "https://foo/?q=foo"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.searchbox_stats.MergeFrom(entry.searchbox_stats);
    search_terms_args.prefetch_param = is_prefetch ? "test" : "";
    search_terms_data_.set_google_base_url(entry.base_url);
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, ReplaceSearchboxStats) {
  // Test the params on non-prefetch requests. kPrefetchParameterFix and
  // kRemoveSearchboxStatsParamFromPrefetchRequests shouldn't affect the
  // results.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {switches::kPrefetchParameterFix,
             switches::kRemoveSearchboxStatsParamFromPrefetchRequests});
    TestReplaceSearcboxStats(false, "", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kPrefetchParameterFix},
        {switches::kRemoveSearchboxStatsParamFromPrefetchRequests});
    TestReplaceSearcboxStats(false, "", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kRemoveSearchboxStatsParamFromPrefetchRequests},
        {switches::kPrefetchParameterFix});
    TestReplaceSearcboxStats(false, "", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kPrefetchParameterFix,
         switches::kRemoveSearchboxStatsParamFromPrefetchRequests},
        {});
    TestReplaceSearcboxStats(false, "", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }

  // Test the params on prefetch requests. kPrefetchParameterFix and
  // kRemoveSearchboxStatsParamFromPrefetchRequests should control the results.
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {switches::kPrefetchParameterFix,
             switches::kRemoveSearchboxStatsParamFromPrefetchRequests});
    TestReplaceSearcboxStats(true, "", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kPrefetchParameterFix},
        {switches::kRemoveSearchboxStatsParamFromPrefetchRequests});
    TestReplaceSearcboxStats(true, "pf=test&", "gs_lcrp=EgZjaHJvbWWwAgE&");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 2);
    histogram_tester.ExpectBucketCount("Omnibox.SearchboxStats.Length", 15, 2);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kRemoveSearchboxStatsParamFromPrefetchRequests},
        {switches::kPrefetchParameterFix});
    TestReplaceSearcboxStats(true, "", "");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 0);
  }
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {switches::kPrefetchParameterFix,
         switches::kRemoveSearchboxStatsParamFromPrefetchRequests},
        {});
    TestReplaceSearcboxStats(true, "pf=test&", "");
    histogram_tester.ExpectTotalCount("Omnibox.SearchboxStats.Length", 0);
  }
}

// Tests replacing cursor position.
TEST_F(TemplateURLTest, ReplaceCursorPosition) {
  struct TestData {
    const std::u16string search_term;
    size_t cursor_position;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      {u"foo", std::u16string::npos,
       "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
       "http://www.google.com/?foo&"},
      {u"foo", 2, "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
       "http://www.google.com/?foo&cp=2&"},
      {u"foo", 15, "{google:baseURL}?{searchTerms}&{google:cursorPosition}",
       "http://www.google.com/?foo&cp=15&"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.cursor_position = entry.cursor_position;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

// Tests replacing input type (&oit=).
TEST_F(TemplateURLTest, ReplaceInputType) {
  struct TestData {
    const std::u16string search_term;
    metrics::OmniboxInputType input_type;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      {u"foo", metrics::OmniboxInputType::UNKNOWN,
       "{google:baseURL}?{searchTerms}&{google:inputType}",
       "http://www.google.com/?foo&oit=1&"},
      {u"foo", metrics::OmniboxInputType::URL,
       "{google:baseURL}?{searchTerms}&{google:inputType}",
       "http://www.google.com/?foo&oit=3&"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.input_type = entry.input_type;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

// Tests replacing omnibox focus type (&oft=).
TEST_F(TemplateURLTest, ReplaceOmniboxFocusType) {
  struct TestData {
    const std::u16string search_term;
    metrics::OmniboxFocusType focus_type;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      {u"foo", metrics::OmniboxFocusType::INTERACTION_DEFAULT,
       "{google:baseURL}?{searchTerms}&{google:omniboxFocusType}",
       "http://www.google.com/?foo&"},
      {u"foo", metrics::OmniboxFocusType::INTERACTION_FOCUS,
       "{google:baseURL}?{searchTerms}&{google:omniboxFocusType}",
       "http://www.google.com/?foo&oft=1&"},
      {u"foo", metrics::OmniboxFocusType::INTERACTION_CLOBBER,
       "{google:baseURL}?{searchTerms}&{google:omniboxFocusType}",
       "http://www.google.com/?foo&oft=2&"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.focus_type = entry.focus_type;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, GetRegulatoryExtensionType) {
  TemplateURLData data;
  {
    TemplateURL url(data);
    EXPECT_EQ(RegulatoryExtensionType::kDefault,
              url.GetRegulatoryExtensionType());
  }

  {
    data.created_from_play_api = true;
    TemplateURL url(data);
    EXPECT_EQ(RegulatoryExtensionType::kAndroidEEA,
              url.GetRegulatoryExtensionType());
  }
}

TEST_F(TemplateURLTest, GetRegulatoryExtension_NoExtension) {
  TemplateURLData data;
  TemplateURL url(data);
  EXPECT_EQ(nullptr,
            url.GetRegulatoryExtension(RegulatoryExtensionType::kDefault));
  EXPECT_EQ(nullptr,
            url.GetRegulatoryExtension(RegulatoryExtensionType::kAndroidEEA));
}

TEST_F(TemplateURLTest, GetRegulatoryExtension_OnlyDefaultExtension) {
  constexpr auto default_ext = TemplateURLData::RegulatoryExtension{
      .variant = RegulatoryExtensionType::kDefault,
      .search_params = "search=params1",
      .suggest_params = "suggest=params2",
  };

  TemplateURLData data;
  data.regulatory_extensions.insert_or_assign(default_ext.variant,
                                              &default_ext);

  // Default extension should give us params mentioned above.
  TemplateURL url(data);
  auto* extension =
      url.GetRegulatoryExtension(RegulatoryExtensionType::kDefault);
  EXPECT_EQ(&default_ext, extension);

  // Android EEA extension should not fall back to default; instead an empty
  // definition should be served.
  extension = url.GetRegulatoryExtension(RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(nullptr, extension);
}

TEST_F(TemplateURLTest, GetRegulatoryExtension_WithDefaultAndEEAExtensions) {
  constexpr auto default_ext = TemplateURLData::RegulatoryExtension{
      .variant = RegulatoryExtensionType::kDefault,
      .search_params = "search=params1",
      .suggest_params = "suggest=params2",
  };
  constexpr auto android_eea_ext = TemplateURLData::RegulatoryExtension{
      .variant = RegulatoryExtensionType::kAndroidEEA,
      .search_params = "eea_search=params3",
      .suggest_params = "eea_suggest=params4",
  };
  TemplateURLData data;
  data.regulatory_extensions.insert_or_assign(default_ext.variant,
                                              &default_ext);
  data.regulatory_extensions.insert_or_assign(android_eea_ext.variant,
                                              &android_eea_ext);

  // Default extension should give us default params.
  TemplateURL url(data);
  auto* extension =
      url.GetRegulatoryExtension(RegulatoryExtensionType::kDefault);
  EXPECT_EQ(&default_ext, extension);

  // Android EEA extension should use android_eea params.
  extension = url.GetRegulatoryExtension(RegulatoryExtensionType::kAndroidEEA);
  EXPECT_EQ(&android_eea_ext, extension);
}

class TemplateURLOnePrefetchSourceTest : public base::test::WithFeatureOverride,
                                         public TemplateURLTest {
 public:
  TemplateURLOnePrefetchSourceTest()
      : base::test::WithFeatureOverride(switches::kPrefetchParameterFix) {}
};
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(TemplateURLOnePrefetchSourceTest);
// Tests replacing prefetch param (&pf=) with assistedQueryStats.
TEST_P(TemplateURLOnePrefetchSourceTest, ReplaceIsPrefetch) {
  struct TestData {
    const std::u16string search_term;
    std::string prefetch_param;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      // Only one of the component works.
      {u"foo", "cs",
       "{google:baseURL}?{searchTerms}&{google:assistedQueryStats}{google:"
       "prefetchSource}",
       "http://www.google.com/?foo&pf=cs&"},
      {u"foo", "",
       "{google:baseURL}?{searchTerms}&{google:assistedQueryStats}{google:"
       "prefetchSource}",
       "http://www.google.com/?foo&"},
      {u"foo", "cs",
       "{google:baseURL}?{searchTerms}&{google:assistedQueryStats}{google:"
       "prefetchSource}",
       "http://www.google.com/?foo&pf=cs&"},
      {u"foo", "op",
       "{google:baseURL}?{searchTerms}&{google:assistedQueryStats}{google:"
       "prefetchSource}",
       "http://www.google.com/?foo&pf=op&"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.prefetch_param = entry.prefetch_param;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

// Tests replacing currentPageUrl.
TEST_F(TemplateURLTest, ReplaceCurrentPageUrl) {
  struct TestData {
    const std::u16string search_term;
    const std::string current_page_url;
    const std::string url;
    const std::string expected_result;
  } test_data[] = {
      {u"foo", "http://www.google.com/",
       "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
       "http://www.google.com/?foo&url=http%3A%2F%2Fwww.google.com%2F&"},
      {u"foo", "", "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
       "http://www.google.com/?foo&"},
      {u"foo", "http://g.com/+-/*&=",
       "{google:baseURL}?{searchTerms}&{google:currentPageUrl}",
       "http://www.google.com/?foo&url=http%3A%2F%2Fg.com%2F%2B-%2F*%26%3D&"},
  };
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    TemplateURLRef::SearchTermsArgs search_terms_args(entry.search_term);
    search_terms_args.current_page_url = entry.current_page_url;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

#if BUILDFLAG(IS_ANDROID)
// Tests appending attribution parameter to queries originating from Play API
// search engine.
TEST_F(TemplateURLTest, PlayAPIAttributionEnabled) {
  const struct TestData {
    const char* url;
    std::u16string terms;
    bool created_from_play_api;
    const char* output;
  } test_data[] = {
      {"http://foo/?q={searchTerms}", u"bar", false, "http://foo/?q=bar"},
      {"http://foo/?q={searchTerms}", u"bar", true,
       "http://foo/?q=bar&chrome_dse_attribution=1"}};
  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    data.created_from_play_api = entry.created_from_play_api;
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(entry.terms), search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.output, result.spec());
  }
}

TEST_F(TemplateURLTest, PlayAPIAttributionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      switches::kRemoveSearchEngineChoiceAttribution);
  const struct TestData {
    const char* url;
    std::u16string terms;
    bool created_from_play_api;
    const char* output;
  } test_data[] = {
      {"http://foo/?q={searchTerms}", u"bar", false, "http://foo/?q=bar"},
      {"http://foo/?q={searchTerms}", u"bar", true, "http://foo/?q=bar"}};
  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.SetURL(entry.url);
    data.created_from_play_api = entry.created_from_play_api;
    TemplateURL url(data);
    EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
    ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
    GURL result(url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(entry.terms), search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.output, result.spec());
  }
}
#endif

TEST_F(TemplateURLTest, Suggestions) {
  struct TestData {
    const int accepted_suggestion;
    const std::u16string original_query_for_suggestion;
    const std::string expected_result;
  } test_data[] = {
      {TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, std::u16string(),
       "http://bar/foo?q=foobar"},
      {TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, u"foo",
       "http://bar/foo?q=foobar"},
      {TemplateURLRef::NO_SUGGESTION_CHOSEN, std::u16string(),
       "http://bar/foo?q=foobar"},
      {TemplateURLRef::NO_SUGGESTION_CHOSEN, u"foo", "http://bar/foo?q=foobar"},
      {0, std::u16string(), "http://bar/foo?oq=&q=foobar"},
      {1, u"foo", "http://bar/foo?oq=foo&q=foobar"},
  };
  TemplateURLData data;
  data.SetURL("http://bar/foo?{google:originalQueryForSuggestion}"
              "q={searchTerms}");
  data.input_encodings.push_back("UTF-8");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  for (const auto& entry : test_data) {
    TemplateURLRef::SearchTermsArgs search_terms_args(u"foobar");
    search_terms_args.accepted_suggestion = entry.accepted_suggestion;
    search_terms_args.original_query = entry.original_query_for_suggestion;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, RLZ) {
  std::u16string rlz_string = search_terms_data_.GetRlzParameterValue(false);

  TemplateURLData data;
  data.SetURL("http://bar/?{google:RLZ}{searchTerms}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  GURL result(url.url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(u"x"), search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://bar/?rlz=" + base::UTF16ToUTF8(rlz_string) + "&x",
            result.spec());
}

TEST_F(TemplateURLTest, RLZFromAppList) {
  std::u16string rlz_string = search_terms_data_.GetRlzParameterValue(true);

  TemplateURLData data;
  data.SetURL("http://bar/?{google:RLZ}{searchTerms}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs args(u"x");
  args.request_source = RequestSource::CROS_APP_LIST;
  GURL result(url.url_ref().ReplaceSearchTerms(args, search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://bar/?rlz=" + base::UTF16ToUTF8(rlz_string) + "&x",
            result.spec());
}

TEST_F(TemplateURLTest, HostAndSearchTermKey) {
  struct TestData {
    const std::string url;
    const std::string host;
    const std::string path;
    const std::string search_term_key;
  } test_data[] = {
      {"http://blah/?foo=bar&q={searchTerms}&b=x", "blah", "/", "q"},
      {"http://blah/{searchTerms}", "blah", "", ""},

      // No term should result in empty values.
      {"http://blah/", "", "", ""},

      // Multiple terms should result in empty values.
      {"http://blah/?q={searchTerms}&x={searchTerms}", "", "", ""},

      // Term in the host shouldn't match.
      {"http://{searchTerms}", "", "", ""},

      {"http://blah/?q={searchTerms}", "blah", "/", "q"},
      {"https://blah/?q={searchTerms}", "blah", "/", "q"},

      // Single term with extra chars in value should match.
      {"http://blah/?q=stock:{searchTerms}", "blah", "/", "q"},
  };

  for (const auto& entry : test_data) {
    TemplateURLData data;
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_EQ(entry.host, url.url_ref().GetHost(search_terms_data_));
    EXPECT_EQ(entry.path, url.url_ref().GetPath(search_terms_data_));
    EXPECT_EQ(entry.search_term_key,
              url.url_ref().GetSearchTermKey(search_terms_data_));
  }
}

TEST_F(TemplateURLTest, SearchTermKeyLocation) {
  struct TestData {
    const std::string url;
    const url::Parsed::ComponentType location;
    const std::string path;
    const std::string key;
    const std::string value_prefix;
    const std::string value_suffix;
  } test_data[] = {
      {"http://blah/{searchTerms}/", url::Parsed::PATH, "", "", "/", "/"},
      {"http://blah/{searchTerms}", url::Parsed::PATH, "", "", "/", ""},
      {"http://blah/begin/{searchTerms}/end", url::Parsed::PATH, "", "",
       "/begin/", "/end"},
      {"http://blah/?foo=bar&q={searchTerms}&b=x", url::Parsed::QUERY, "/", "q",
       "", ""},
      {"http://blah/?foo=bar#x={searchTerms}&b=x", url::Parsed::REF, "/", "x",
       "", ""},
      {"http://www.example.com/?q=chromium-{searchTerms}@chromium.org/info",
       url::Parsed::QUERY, "/", "q", "chromium-", "@chromium.org/info"},

      // searchTerms is a key, not a value, so this should result in an empty
      // value.
      {"http://blah/?foo=bar#x=012345678901234&a=b&{searchTerms}=x",
       url::Parsed::QUERY, "", "", "", ""},

      // Multiple search terms should result in empty values.
      {"http://blah/{searchTerms}?q={searchTerms}", url::Parsed::QUERY, "", "",
       "", ""},
      {"http://blah/{searchTerms}#x={searchTerms}", url::Parsed::QUERY, "", "",
       "", ""},
      {"http://blah/?q={searchTerms}#x={searchTerms}", url::Parsed::QUERY, "",
       "", "", ""},
  };

  for (const auto& entry : test_data) {
    TemplateURLData data;
    data.SetURL(entry.url);
    TemplateURL url(data);
    EXPECT_EQ(entry.location,
              url.url_ref().GetSearchTermKeyLocation(search_terms_data_));
    EXPECT_EQ(entry.path, url.url_ref().GetPath(search_terms_data_));
    EXPECT_EQ(entry.key, url.url_ref().GetSearchTermKey(search_terms_data_));
    EXPECT_EQ(entry.value_prefix,
              url.url_ref().GetSearchTermValuePrefix(search_terms_data_));
    EXPECT_EQ(entry.value_suffix,
              url.url_ref().GetSearchTermValueSuffix(search_terms_data_));
  }
}

TEST_F(TemplateURLTest, GoogleBaseSuggestURL) {
  static const struct {
    const char* const base_url;
    const char* const base_suggest_url;
  } data[] = {
    { "http://google.com/", "http://google.com/complete/", },
    { "http://www.google.com/", "http://www.google.com/complete/", },
    { "http://www.google.co.uk/", "http://www.google.co.uk/complete/", },
    { "http://www.google.com.by/", "http://www.google.com.by/complete/", },
    { "http://google.com/intl/xx/", "http://google.com/complete/", },
  };

  for (size_t i = 0; i < std::size(data); ++i)
    CheckSuggestBaseURL(data[i].base_url, data[i].base_suggest_url);
}

TEST_F(TemplateURLTest, ParseParameterKnown) {
  std::string parsed_url("{searchTerms}");
  TemplateURLData data;
  data.SetURL(parsed_url);
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  EXPECT_TRUE(url.url_ref().ParseParameter(0, 12, &parsed_url, &replacements));
  EXPECT_EQ(std::string(), parsed_url);
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(0U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
}

TEST_F(TemplateURLTest, ParseParameterUnknown) {
  std::string parsed_url("{fhqwhgads}abc");
  TemplateURLData data;
  data.SetURL(parsed_url);
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;

  // By default, TemplateURLRef should not consider itself prepopulated.
  // Therefore we should not replace the unknown parameter.
  EXPECT_TRUE(url.url_ref().ParseParameter(0, 10, &parsed_url, &replacements));
  EXPECT_EQ("abc", parsed_url);
  EXPECT_TRUE(replacements.empty());

  // If the TemplateURLRef is prepopulated, we should remove unknown parameters.
  parsed_url = "{fhqwhgads}abc";
  data.prepopulate_id = 1;
  TemplateURL url2(data);
  url2.url_ref().ParseParameter(0, 10, &parsed_url, &replacements);
  EXPECT_TRUE(replacements.empty());
}

TEST_F(TemplateURLTest, ParseURLEmpty) {
  TemplateURL url((TemplateURLData()));
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(), url.url_ref().ParseURL(std::string(), &replacements,
                                                  nullptr, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoTemplateEnd) {
  TemplateURLData data;
  data.SetURL("{");
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ(std::string(),
            url.url_ref().ParseURL("{", &replacements, nullptr, &valid));
  EXPECT_TRUE(replacements.empty());
  EXPECT_FALSE(valid);
}

TEST_F(TemplateURLTest, ParseURLNoKnownParameters) {
  TemplateURLData data;
  data.SetURL("{}");
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;

  EXPECT_EQ("", url.url_ref().ParseURL("{}", &replacements, nullptr, &valid));
  EXPECT_TRUE(valid);
  EXPECT_TRUE(replacements.empty());
}

TEST_F(TemplateURLTest, ParseURLTwoParameters) {
  TemplateURLData data;
  data.SetURL("{}{{%s}}");
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{}", url.url_ref().ParseURL("{{searchTerms}}", &replacements,
                                         nullptr, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(1U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, ParseURLNestedParameter) {
  TemplateURLData data;
  data.SetURL("{%s");
  TemplateURL url(data);
  TemplateURLRef::Replacements replacements;
  bool valid = false;
  EXPECT_EQ("{", url.url_ref().ParseURL("{{searchTerms}", &replacements,
                                        nullptr, &valid));
  ASSERT_EQ(1U, replacements.size());
  EXPECT_EQ(1U, replacements[0].index);
  EXPECT_EQ(TemplateURLRef::SEARCH_TERMS, replacements[0].type);
  EXPECT_TRUE(valid);
}

TEST_F(TemplateURLTest, SearchSourceId) {
  const std::string base_url_str("http://google.com/?");
  const std::string query_params_str("{google:sourceId}");
  const std::string full_url_str = base_url_str + query_params_str;
  search_terms_data_.set_google_base_url(base_url_str);

  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_FALSE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs search_terms_args;

  // Check that the URL is correct for the default `RequestSource::SEARCHBOX`.
  GURL result(
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_));
  ASSERT_TRUE(result.is_valid());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ("http://google.com/?sourceid=chrome-mobile&", result.spec());
#else
  EXPECT_EQ("http://google.com/?sourceid=chrome&", result.spec());
#endif
}

TEST_F(TemplateURLTest, SearchClient) {
  const std::string base_url_str("http://google.com/?");
  const std::string terms_str("{searchTerms}&{google:searchClient}");
  const std::string full_url_str = base_url_str + terms_str;
  const std::u16string terms(ASCIIToUTF16(terms_str));
  search_terms_data_.set_google_base_url(base_url_str);

  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foobar");

  // Check that the URL is correct when a client is not present.
  GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                               search_terms_data_));
  ASSERT_TRUE(result.is_valid());
  EXPECT_EQ("http://google.com/?foobar&", result.spec());

  // Check that the URL is correct when a client is present.
  search_terms_data_.set_search_client("search_client");
  GURL result_2(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
  ASSERT_TRUE(result_2.is_valid());
  EXPECT_EQ("http://google.com/?foobar&client=search_client&", result_2.spec());
}

TEST_F(TemplateURLTest, SuggestClient) {
  const std::string base_url_str("http://google.com/?");
  const std::string query_params_str("client={google:suggestClient}");
  const std::string full_url_str = base_url_str + query_params_str;
  search_terms_data_.set_google_base_url(base_url_str);

  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_FALSE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs search_terms_args;

  // Check that the URL is correct for the default `RequestSource::SEARCHBOX`.
  GURL result(
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_));
  ASSERT_TRUE(result.is_valid());
#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    EXPECT_EQ("http://google.com/?client=chrome", result.spec());
  } else {
    EXPECT_EQ("http://google.com/?client=chrome-omni", result.spec());
  }
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ("http://google.com/?client=chrome", result.spec());
#else
  EXPECT_EQ("http://google.com/?client=chrome-omni", result.spec());
#endif
}

TEST_F(TemplateURLTest, SuggestRequestIdentifier) {
  const std::string base_url_str("http://google.com/?");
  const std::string query_params_str("gs_ri={google:suggestRid}");
  const std::string full_url_str = base_url_str + query_params_str;
  search_terms_data_.set_google_base_url(base_url_str);

  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_FALSE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs search_terms_args;

  // Check that the URL is correct for the default `RequestSource::SEARCHBOX`.
  GURL result(
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_));
  ASSERT_TRUE(result.is_valid());
#if BUILDFLAG(IS_ANDROID)
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE) {
    EXPECT_EQ("http://google.com/?gs_ri=chrome-mobile-ext-ansg", result.spec());
  } else {
    EXPECT_EQ("http://google.com/?gs_ri=chrome-ext-ansg", result.spec());
  }
#else
  EXPECT_EQ("http://google.com/?gs_ri=chrome-ext-ansg", result.spec());
#endif
}

TEST_F(TemplateURLTest, ZeroSuggestCacheDuration) {
  const std::string base_url_str("http://google.com/?");
  const std::string query_params_str("{google:clientCacheTimeToLive}");
  const std::string full_url_str = base_url_str + query_params_str;
  search_terms_data_.set_google_base_url(base_url_str);
  TemplateURLData data;
  data.SetURL(full_url_str);
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_FALSE(url.url_ref().SupportsReplacement(search_terms_data_));

  {
    // 'ccttl' query param should not be present if no cache duration is given.
    TemplateURLRef::SearchTermsArgs search_terms_args;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ("http://google.com/?", result.spec());
  }
  {
    // 'ccttl' query param should be present if a positive cache duration is
    // given.
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.zero_suggest_cache_duration_sec = 300;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ("http://google.com/?ccttl=300&", result.spec());
  }
  {
    // 'ccttl' query param shouldn't be present if a zero cache duration is
    // given.
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.zero_suggest_cache_duration_sec = 0;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ("http://google.com/?", result.spec());
  }
  {
    // 'ccttl' query param should not be present if the request is not a
    // zero-prefix request.
    TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
    search_terms_args.zero_suggest_cache_duration_sec = 300;
    GURL result(url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ("http://google.com/?", result.spec());
  }
}

TEST_F(TemplateURLTest, GetURLNoSuggestionsURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.alternate_urls.push_back("http://google.com/alt?q={searchTerms}");
  data.alternate_urls.push_back("{google:baseURL}/alt/#q={searchTerms}");
  TemplateURL url(data);
  const std::vector<TemplateURLRef>& url_refs = url.url_refs();
  ASSERT_EQ(3U, url_refs.size());
  EXPECT_EQ("http://google.com/alt?q={searchTerms}", url_refs[0].GetURL());
  EXPECT_EQ("{google:baseURL}/alt/#q={searchTerms}", url_refs[1].GetURL());
  EXPECT_EQ("http://google.com/?q={searchTerms}", url_refs[2].GetURL());
}

TEST_F(TemplateURLTest, GetURLOnlyOneURL) {
  TemplateURLData data;
  data.SetURL("http://www.google.co.uk/");
  TemplateURL url(data);
  const std::vector<TemplateURLRef>& url_refs = url.url_refs();
  ASSERT_EQ(1U, url_refs.size());
  EXPECT_EQ("http://www.google.co.uk/", url_refs[0].GetURL());
}

TEST_F(TemplateURLTest, ExtractSearchTermsFromURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.alternate_urls.push_back("http://google.com/alt/#q={searchTerms}");
  data.alternate_urls.push_back(
      "http://google.com/alt/?ext=foo&q={searchTerms}#ref=bar");
  TemplateURL url(data);
  std::u16string result;

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#q=something"),
      search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/#q=something"), search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.ca/?q=something"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.ca/?q=something&q=anything"),
      search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/foo/?q=foo"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("https://google.com/?q=foo"), search_terms_data_, &result));
  EXPECT_EQ(u"foo", result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com:8080/?q=foo"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/?q=1+2+3&b=456"), search_terms_data_, &result));
  EXPECT_EQ(u"1 2 3", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=123#q=456"),
      search_terms_data_, &result));
  EXPECT_EQ(u"456", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#f=789"),
      search_terms_data_, &result));
  EXPECT_EQ(u"123", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(GURL(
      "http://google.com/alt/?a=012&q=123&b=456#j=abc&q=789&h=def9"),
                                            search_terms_data_, &result));
  EXPECT_EQ(u"789", result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q="), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?#q="), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=#q="), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=123#q="), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://google.com/alt/?q=#q=123"), search_terms_data_, &result));
  EXPECT_EQ(u"123", result);
}

TEST_F(TemplateURLTest, ExtractSearchTermsFromURLPath) {
  TemplateURLData data;
  data.SetURL("http://term-in-path.com/begin/{searchTerms}/end");
  TemplateURL url(data);
  std::u16string result;

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/begin/something/end"),
      search_terms_data_, &result));
  EXPECT_EQ(u"something", result);

  // "%20" must be converted to space.
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/begin/a%20b%20c/end"),
      search_terms_data_, &result));
  EXPECT_EQ(u"a b c", result);

  // Plus must not be converted to space.
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/begin/1+2+3/end"),
      search_terms_data_, &result));
  EXPECT_EQ(u"1+2+3", result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/about"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/begin"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);

  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://term-in-path.com/end"), search_terms_data_, &result));
  EXPECT_EQ(std::u16string(), result);
}

// Checks that the ExtractSearchTermsFromURL function works correctly
// for urls containing non-latin characters in UTF8 encoding.
TEST_F(TemplateURLTest, ExtractSearchTermsFromUTF8URL) {
  TemplateURLData data;
  data.SetURL("http://utf-8.ru/?q={searchTerms}");
  data.alternate_urls.push_back("http://utf-8.ru/#q={searchTerms}");
  data.alternate_urls.push_back("http://utf-8.ru/path/{searchTerms}");
  TemplateURL url(data);
  std::u16string result;

  // Russian text encoded with UTF-8.
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://utf-8.ru/?q=%D0%97%D0%B4%D1%80%D0%B0%D0%B2%D1%81%D1%82"
           "%D0%B2%D1%83%D0%B9,+%D0%BC%D0%B8%D1%80!"),
      search_terms_data_, &result));
  EXPECT_EQ(
      u"\x0417\x0434\x0440\x0430\x0432\x0441\x0442\x0432\x0443\x0439, "
      u"\x043C\x0438\x0440!",
      result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://utf-8.ru/#q=%D0%B4%D0%B2%D0%B0+%D1%81%D0%BB%D0%BE%D0%B2"
           "%D0%B0"),
      search_terms_data_, &result));
  EXPECT_EQ(u"\x0434\x0432\x0430 \x0441\x043B\x043E\x0432\x0430", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://utf-8.ru/path/%D0%B1%D1%83%D0%BA%D0%B2%D1%8B%20%D0%90%20"
           "%D0%B8%20A"),
      search_terms_data_, &result));
  EXPECT_EQ(u"\x0431\x0443\x043A\x0432\x044B \x0410 \x0438 A", result);
}

// Checks that the ExtractSearchTermsFromURL function works correctly
// for urls containing non-latin characters in non-UTF8 encoding.
TEST_F(TemplateURLTest, ExtractSearchTermsFromNonUTF8URL) {
  TemplateURLData data;
  data.SetURL("http://windows-1251.ru/?q={searchTerms}");
  data.alternate_urls.push_back("http://windows-1251.ru/#q={searchTerms}");
  data.alternate_urls.push_back("http://windows-1251.ru/path/{searchTerms}");
  data.input_encodings.push_back("windows-1251");
  TemplateURL url(data);
  std::u16string result;

  // Russian text encoded with Windows-1251.
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://windows-1251.ru/?q=%C7%E4%F0%E0%E2%F1%F2%E2%F3%E9%2C+"
           "%EC%E8%F0!"),
      search_terms_data_, &result));
  EXPECT_EQ(
      u"\x0417\x0434\x0440\x0430\x0432\x0441\x0442\x0432\x0443\x0439, "
      u"\x043C\x0438\x0440!",
      result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://windows-1251.ru/#q=%E4%E2%E0+%F1%EB%EE%E2%E0"),
      search_terms_data_, &result));
  EXPECT_EQ(u"\x0434\x0432\x0430 \x0441\x043B\x043E\x0432\x0430", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://windows-1251.ru/path/%E1%F3%EA%E2%FB%20%C0%20%E8%20A"),
      search_terms_data_, &result));
  EXPECT_EQ(u"\x0431\x0443\x043A\x0432\x044B \x0410 \x0438 A", result);
}

// Checks that the ExtractSearchTermsFromURL function strips constant
// prefix/suffix strings from the search terms param.
TEST_F(TemplateURLTest, ExtractSearchTermsWithPrefixAndSuffix) {
  TemplateURLData data;
  data.alternate_urls.push_back(
      "http://www.example.com/?q=chromium-{searchTerms}@chromium.org");
  data.alternate_urls.push_back(
      "http://www.example.com/chromium-{searchTerms}@chromium.org/info");
  TemplateURL url(data);
  std::u16string result;

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://www.example.com/?q=chromium-dev@chromium.org"),
      search_terms_data_, &result));
  EXPECT_EQ(u"dev", result);

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://www.example.com/chromium-dev@chromium.org/info"),
      search_terms_data_, &result));
  EXPECT_EQ(u"dev", result);

  // Don't match if the prefix and suffix aren't there.
  EXPECT_FALSE(url.ExtractSearchTermsFromURL(
      GURL("http://www.example.com/?q=invalid"), search_terms_data_, &result));

  // Don't match if the prefix and suffix overlap.
  TemplateURLData data_with_overlap;
  data.alternate_urls.push_back(
      "http://www.example.com/?q=goo{searchTerms}oogle");
  TemplateURL url_with_overlap(data);
  EXPECT_FALSE(url_with_overlap.ExtractSearchTermsFromURL(
      GURL("http://www.example.com/?q=google"), search_terms_data_, &result));
}

TEST_F(TemplateURLTest, ReplaceSearchTermsInURL) {
  TemplateURLData data;
  data.SetURL("http://google.com/?q={searchTerms}");
  data.alternate_urls.push_back("http://google.com/alt/#q={searchTerms}");
  data.alternate_urls.push_back(
      "http://google.com/alt/?ext=foo&q={searchTerms}#ref=bar");
  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms(u"Bob Morane");
  GURL result;

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/?q=something"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/?q=Bob+Morane"), result);

  result = GURL("http://should.not.change.com");
  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.ca/?q=something"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://should.not.change.com"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/foo/?q=foo"), search_terms,
      search_terms_data_, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("https://google.com/?q=foo"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("https://google.com/?q=Bob+Morane"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com:8080/?q=foo"), search_terms,
      search_terms_data_, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/?q=1+2+3&b=456"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/?q=Bob+Morane&b=456"), result);

  // Note: Spaces in REF parameters are not escaped. See TryEncoding() in
  // template_url.cc for details.
  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=123#q=456"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?q=123#q=Bob+Morane"), result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#f=789"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?a=012&q=Bob+Morane&b=456#f=789"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?a=012&q=123&b=456#j=abc&q=789&h=def9"),
      search_terms, search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?a=012&q=123&b=456"
                 "#j=abc&q=Bob+Morane&h=def9"), result);

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q="), search_terms,
      search_terms_data_, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?#q="), search_terms,
      search_terms_data_, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=#q="), search_terms,
      search_terms_data_, &result));

  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=123#q="), search_terms,
      search_terms_data_, &result));

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://google.com/alt/?q=#q=123"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://google.com/alt/?q=#q=Bob+Morane"), result);
}

TEST_F(TemplateURLTest, ReplaceSearchTermsInURLPath) {
  TemplateURLData data;
  data.SetURL("http://term-in-path.com/begin/{searchTerms}/end");
  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms(u"Bob Morane");
  GURL result;

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://term-in-path.com/begin/something/end"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://term-in-path.com/begin/Bob%20Morane/end"), result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://term-in-path.com/begin/1%202%203/end"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://term-in-path.com/begin/Bob%20Morane/end"), result);

  result = GURL("http://should.not.change.com");
  EXPECT_FALSE(url.ReplaceSearchTermsInURL(
      GURL("http://term-in-path.com/about"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://should.not.change.com"), result);
}

// Checks that the ReplaceSearchTermsInURL function works correctly
// for search terms containing non-latin characters for a search engine
// using UTF-8 input encoding.
TEST_F(TemplateURLTest, ReplaceSearchTermsInUTF8URL) {
  TemplateURLData data;
  data.SetURL("http://utf-8.ru/?q={searchTerms}");
  data.alternate_urls.push_back("http://utf-8.ru/#q={searchTerms}");
  data.alternate_urls.push_back("http://utf-8.ru/path/{searchTerms}");
  TemplateURL url(data);

  // Russian text which will be encoded with UTF-8.
  TemplateURLRef::SearchTermsArgs search_terms(
      u"\x0442\x0435\x043A\x0441\x0442");
  GURL result;

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://utf-8.ru/?q=a+b"), search_terms, search_terms_data_,
      &result));
  EXPECT_EQ(GURL("http://utf-8.ru/?q=%D1%82%D0%B5%D0%BA%D1%81%D1%82"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://utf-8.ru/#q=a+b"), search_terms, search_terms_data_,
      &result));
  EXPECT_EQ(GURL("http://utf-8.ru/#q=%D1%82%D0%B5%D0%BA%D1%81%D1%82"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://utf-8.ru/path/a%20b"), search_terms, search_terms_data_,
      &result));
  EXPECT_EQ(GURL("http://utf-8.ru/path/%D1%82%D0%B5%D0%BA%D1%81%D1%82"),
            result);
}

// Checks that the ReplaceSearchTermsInURL function works correctly
// for search terms containing non-latin characters for a search engine
// using non UTF-8 input encoding.
TEST_F(TemplateURLTest, ReplaceSearchTermsInNonUTF8URL) {
  TemplateURLData data;
  data.SetURL("http://windows-1251.ru/?q={searchTerms}");
  data.alternate_urls.push_back("http://windows-1251.ru/#q={searchTerms}");
  data.alternate_urls.push_back("http://windows-1251.ru/path/{searchTerms}");
  data.input_encodings.push_back("windows-1251");
  TemplateURL url(data);

  // Russian text which will be encoded with Windows-1251.
  TemplateURLRef::SearchTermsArgs search_terms(
      u"\x0442\x0435\x043A\x0441\x0442");
  GURL result;

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://windows-1251.ru/?q=a+b"), search_terms, search_terms_data_,
      &result));
  EXPECT_EQ(GURL("http://windows-1251.ru/?q=%F2%E5%EA%F1%F2"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://windows-1251.ru/#q=a+b"), search_terms, search_terms_data_,
      &result));
  EXPECT_EQ(GURL("http://windows-1251.ru/#q=%F2%E5%EA%F1%F2"),
            result);

  EXPECT_TRUE(url.ReplaceSearchTermsInURL(
      GURL("http://windows-1251.ru/path/a%20b"), search_terms,
      search_terms_data_, &result));
  EXPECT_EQ(GURL("http://windows-1251.ru/path/%F2%E5%EA%F1%F2"),
            result);
}

// Test the |additional_query_params| field of SearchTermsArgs.
TEST_F(TemplateURLTest, SuggestQueryParams) {
  TemplateURLData data;
  // Pick a URL with replacements before, during, and after the query, to ensure
  // we don't goof up any of them.
  data.SetURL("{google:baseURL}search?q={searchTerms}"
      "#{google:originalQueryForSuggestion}x");
  TemplateURL url(data);

  // Baseline: no |additional_query_params| field.
  TemplateURLRef::SearchTermsArgs search_terms(u"abc");
  search_terms.original_query = u"def";
  search_terms.accepted_suggestion = 0;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));

  // Set the |additional_query_params|.
  search_terms.additional_query_params = "pq=xyz";
  EXPECT_EQ("http://www.google.com/search?pq=xyz&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));

  // Add |append_extra_query_params_from_command_line| into the mix, and ensure
  // it works.
  search_terms.append_extra_query_params_from_command_line = true;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  EXPECT_EQ("http://www.google.com/search?a=b&pq=xyz&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));
}

// Test the |search_terms.append_extra_query_params_from_command_line| field of
// SearchTermsArgs.
TEST_F(TemplateURLTest, ExtraQueryParams) {
  TemplateURLData data;
  // Pick a URL with replacements before, during, and after the query, to ensure
  // we don't goof up any of them.
  data.SetURL("{google:baseURL}search?q={searchTerms}"
      "#{google:originalQueryForSuggestion}x");
  TemplateURL url(data);

  // Baseline: no command-line args, no
  // |search_terms.append_extra_query_params_from_command_line| flag.
  TemplateURLRef::SearchTermsArgs search_terms(u"abc");
  search_terms.original_query = u"def";
  search_terms.accepted_suggestion = 0;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));

  // Set the flag.  Since there are no command-line args, this should have no
  // effect.
  search_terms.append_extra_query_params_from_command_line = true;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));

  // Now append the command-line arg.  This should be inserted into the query.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");
  EXPECT_EQ("http://www.google.com/search?a=b&q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));

  // Turn off the flag.  Now the command-line arg should be ignored again.
  search_terms.append_extra_query_params_from_command_line = false;
  EXPECT_EQ("http://www.google.com/search?q=abc#oq=def&x",
            url.url_ref().ReplaceSearchTerms(search_terms, search_terms_data_));
}

// Tests replacing pageClassification.
TEST_F(TemplateURLTest, ReplacePageClassification) {
  TemplateURLData data;
  data.input_encodings.push_back("UTF-8");
  data.SetURL("{google:baseURL}?{google:pageClassification}q={searchTerms}");
  TemplateURL url(data);
  EXPECT_TRUE(url.url_ref().IsValid(search_terms_data_));
  ASSERT_TRUE(url.url_ref().SupportsReplacement(search_terms_data_));
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");

  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                        search_terms_data_);
  EXPECT_EQ("http://www.google.com/?q=foo", result);

  search_terms_args.page_classification = metrics::OmniboxEventProto::NTP;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                            search_terms_data_);
  EXPECT_EQ("http://www.google.com/?pgcl=1&q=foo", result);

  search_terms_args.page_classification =
      metrics::OmniboxEventProto::HOME_PAGE;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                            search_terms_data_);
  EXPECT_EQ("http://www.google.com/?pgcl=3&q=foo", result);
}

// Test the IsSearchURL function.
TEST_F(TemplateURLTest, IsSearchURL) {
  TemplateURLData data;
  data.SetURL("http://bar/search?q={searchTerms}");
  data.new_tab_url = "http://bar/newtab";
  data.alternate_urls.push_back("http://bar/?q={searchTerms}");
  data.alternate_urls.push_back("http://bar/#q={searchTerms}");
  data.alternate_urls.push_back("http://bar/search#q{searchTerms}");
  data.alternate_urls.push_back("http://bar/webhp#q={searchTerms}");
  TemplateURL search_provider(data);

  const struct {
    const char* const url;
    bool result;
  } url_data[] = {
    { "http://bar/search?q=foo&oq=foo", true, },
    { "http://bar/?q=foo&oq=foo", true, },
    { "http://bar/#output=search&q=foo&oq=foo", true, },
    { "http://bar/webhp#q=foo&oq=foo", true, },
    { "http://bar/#q=foo&oq=foo", true, },
    { "http://bar/?ext=foo&q=foo#ref=bar", true, },
    { "http://bar/url?url=http://www.foo.com/&q=foo#ref=bar", false, },
    { "http://bar/", false, },
    { "http://foo/", false, },
    { "http://bar/newtab", false, },
  };

  for (size_t i = 0; i < std::size(url_data); ++i) {
    EXPECT_EQ(url_data[i].result,
              search_provider.IsSearchURL(GURL(url_data[i].url),
                                          search_terms_data_));
  }
}

TEST_F(TemplateURLTest, SearchboxVersionIncludedForAnswers) {
  TemplateURLData data;
  search_terms_data_.set_google_base_url("http://bar/");
  data.SetURL("http://bar/search?q={searchTerms}&{google:searchVersion}xssi=t");

  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                        search_terms_data_);
  EXPECT_EQ("http://bar/search?q=foo&gs_rn=42&xssi=t", result);
}

TEST_F(TemplateURLTest, SessionToken) {
  TemplateURLData data;
  search_terms_data_.set_google_base_url("http://bar/");
  data.SetURL("http://bar/search?q={searchTerms}&{google:sessionToken}xssi=t");

  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
  search_terms_args.session_token = "SESSIONTOKENGOESHERE";
  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                        search_terms_data_);
  EXPECT_EQ("http://bar/search?q=foo&psi=SESSIONTOKENGOESHERE&xssi=t", result);

  TemplateURL url2(data);
  search_terms_args.session_token = "";
  result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                            search_terms_data_);
  EXPECT_EQ("http://bar/search?q=foo&xssi=t", result);
}

TEST_F(TemplateURLTest, ContextualSearchParameters) {
  TemplateURLData data;
  search_terms_data_.set_google_base_url("http://bar/");
  data.SetURL("http://bar/_/contextualsearch?"
              "{google:contextualSearchVersion}"
              "{google:contextualSearchContextData}");

  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
  std::string result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                                        search_terms_data_);
  EXPECT_EQ("http://bar/_/contextualsearch?", result);

  // Test the current common case, which uses no home country or previous
  // event.
  TemplateURLRef::SearchTermsArgs::ContextualSearchParams params(
      2, 1, std::string(), 0, 0, false, std::string(), std::string(),
      std::string(), std::string(), false);
  search_terms_args.contextual_search_params = params;
  result = url.url_ref().ReplaceSearchTerms(search_terms_args,
                                            search_terms_data_);
  EXPECT_EQ(
      "http://bar/_/contextualsearch?"
      "ctxs=2&"
      "ctxsl_coca=1",
      result);

  // Test the home country and non-zero event data case.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 2, "CH", 1657713458, 5, false, std::string(), std::string(),
          std::string(), std::string(), false);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);

  EXPECT_EQ(
      "http://bar/_/contextualsearch?"
      "ctxs=2&"
      "ctxsl_coca=2&"
      "ctxs_hc=CH&"
      "ctxsl_pid=1657713458&"
      "ctxsl_per=5",
      result);

  // Test exact-search.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 1, std::string(), 0, 0, true, std::string(), std::string(),
          std::string(), std::string(), false);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  // Find our param.
  size_t found_pos = result.find("ctxsl_exact=1");
  EXPECT_NE(found_pos, std::string::npos);

  // Test source and target languages.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 1, std::string(), 0, 0, true, "es", "de", std::string(),
          std::string(), false);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  // Find our params.
  size_t source_pos = result.find("tlitesl=es");
  EXPECT_NE(source_pos, std::string::npos);
  size_t target_pos = result.find("tlitetl=de");
  EXPECT_NE(target_pos, std::string::npos);

  // Test fluent languages.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 1, std::string(), 0, 0, true, std::string(), std::string(),
          "es,de", std::string(), false);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  // Find our param.  These may actually be URL encoded.
  size_t fluent_pos = result.find("&ctxs_fls=es,de");
  EXPECT_NE(fluent_pos, std::string::npos);

  // Test Related Searches.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 1, std::string(), 0, 0, true, std::string(), std::string(),
          std::string(), "1RbCu", false);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  // Find our param.
  size_t ctxsl_rs_pos = result.find("&ctxsl_rs=1RbCu");
  EXPECT_NE(ctxsl_rs_pos, std::string::npos);

  // Test apply language hint.
  search_terms_args.contextual_search_params =
      TemplateURLRef::SearchTermsArgs::ContextualSearchParams(
          2, 1, std::string(), 0, 0, true, std::string(), std::string(),
          std::string(), std::string(), true);
  result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  // Find our param.
  size_t ctxsl_applylh = result.find("&ctxsl_applylh=1");
  EXPECT_NE(ctxsl_applylh, std::string::npos);
}

TEST_F(TemplateURLTest, GenerateKeyword) {
  ASSERT_EQ(u"foo", TemplateURL::GenerateKeyword(GURL("http://foo")));
  ASSERT_EQ(u"foo.", TemplateURL::GenerateKeyword(GURL("http://foo.")));
  // www. should be stripped for a public hostname but not a private/intranet
  // hostname.
  ASSERT_EQ(u"google.com",
            TemplateURL::GenerateKeyword(GURL("http://www.google.com")));
  ASSERT_EQ(u"www.foo", TemplateURL::GenerateKeyword(GURL("http://www.foo")));
  // Make sure we don't get a trailing '/'.
  ASSERT_EQ(u"blah", TemplateURL::GenerateKeyword(GURL("http://blah/")));
  // Don't generate the empty string.
  ASSERT_EQ(u"www.", TemplateURL::GenerateKeyword(GURL("http://www.")));
  ASSERT_EQ(u"абв", TemplateURL::GenerateKeyword(GURL("http://xn--80acd")));

  // Generated keywords must always be in lowercase, because TemplateURLs always
  // converts keywords to lowercase in its constructor and TemplateURLService
  // stores TemplateURLs in maps using keyword as key.
  EXPECT_TRUE(IsLowerCase(TemplateURL::GenerateKeyword(GURL("http://BLAH/"))));
  EXPECT_TRUE(IsLowerCase(
      TemplateURL::GenerateKeyword(GURL("http://embeddedhtml.-head-/"))));
}

TEST_F(TemplateURLTest, KeepSearchTermsInURL) {
  search_terms_data_.set_google_base_url("http://bar/");

  TemplateURLData data;
  data.SetURL("http://bar/search?q={searchTerms}&{google:sessionToken}xssi=t");
  data.search_intent_params = {"gs_ssp", "si"};
  TemplateURL turl(data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"FOO");
  search_terms_args.session_token = "SESSIONTOKEN";

  {
    // Optionally keeps non-empty search intent params.
    search_terms_args.additional_query_params = "gs_ssp=GS_SSP";
    std::string original_search_url = turl.url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_);
    EXPECT_EQ("http://bar/search?gs_ssp=GS_SSP&q=FOO&psi=SESSIONTOKEN&xssi=t",
              original_search_url);

    GURL canonical_search_url;
    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/true, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?gs_ssp=GS_SSP&q=foo&xssi=t",
              canonical_search_url);

    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?q=foo&xssi=t", canonical_search_url);
  }
  {
    // Optionally keeps empty search intent params.
    search_terms_args.additional_query_params = "gs_ssp=";
    std::string original_search_url = turl.url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_);
    EXPECT_EQ("http://bar/search?gs_ssp=&q=FOO&psi=SESSIONTOKEN&xssi=t",
              original_search_url);

    GURL canonical_search_url;
    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/true, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?gs_ssp=&q=foo&xssi=t", canonical_search_url);

    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?q=foo&xssi=t", canonical_search_url);
  }
  {
    // Discards params besides search terms and optionally search intent params.
    search_terms_args.additional_query_params = "wiz=baz&gs_ssp=GS_SSP";
    std::string original_search_url = turl.url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_);
    EXPECT_EQ(
        "http://bar/search?wiz=baz&gs_ssp=GS_SSP&q=FOO&psi=SESSIONTOKEN&xssi=t",
        original_search_url);

    GURL canonical_search_url;
    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/true, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?gs_ssp=GS_SSP&q=foo&xssi=t",
              canonical_search_url);

    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?q=foo&xssi=t", canonical_search_url);
  }
  {
    // Optionally keeps multiple search intent params.
    search_terms_args.additional_query_params = "si=SI&gs_ssp=GS_SSP";
    std::string original_search_url = turl.url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_);
    EXPECT_EQ(
        "http://bar/search?si=SI&gs_ssp=GS_SSP&q=FOO&psi=SESSIONTOKEN&xssi=t",
        original_search_url);

    GURL canonical_search_url;
    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/true, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?si=SI&gs_ssp=GS_SSP&q=foo&xssi=t",
              canonical_search_url);

    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/true,
        &canonical_search_url));
    EXPECT_EQ("http://bar/search?q=foo&xssi=t", canonical_search_url);
  }
  {
    // Search terms extraction, normalized or not.
    search_terms_args.additional_query_params = "gs_ssp=GS_SSP";
    std::string original_search_url = turl.url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_);
    EXPECT_EQ("http://bar/search?gs_ssp=GS_SSP&q=FOO&psi=SESSIONTOKEN&xssi=t",
              original_search_url);

    GURL canonical_search_url;
    std::u16string search_terms;
    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/true,
        &canonical_search_url, &search_terms));
    EXPECT_EQ("http://bar/search?q=foo&xssi=t", canonical_search_url);
    EXPECT_EQ(u"foo", search_terms);

    EXPECT_TRUE(turl.KeepSearchTermsInURL(
        GURL(original_search_url), search_terms_data_,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/false,
        &canonical_search_url, &search_terms));
    EXPECT_EQ("http://bar/search?q=FOO&xssi=t", canonical_search_url);
    EXPECT_EQ(u"FOO", search_terms);
  }
}

TEST_F(TemplateURLTest, GenerateSearchURL) {
  struct GenerateSearchURLCase {
    const char* test_name;
    const char* url;
    const char* expected;
  } generate_url_cases[] = {
    { "invalid URL", "foo{searchTerms}", "" },
    { "URL with no replacements", "http://foo/", "http://foo/" },
    { "basic functionality", "http://foo/{searchTerms}",
      "http://foo/blah.blah.blah.blah.blah" }
  };

  for (const auto& generate_url_case : generate_url_cases) {
    TemplateURLData data;
    data.SetURL(generate_url_case.url);
    TemplateURL t_url(data);
    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_).spec(),
              generate_url_case.expected)
        << generate_url_case.test_name << " failed.";
  }
}

TEST_F(TemplateURLTest, GenerateURL_NoRegulatoryExtensions) {
  TemplateURLData data;
  data.SetURL("https://search?q={searchTerms}");
  data.suggestions_url = "https://suggest?q={searchTerms}";

  {
    TemplateURL t_url(data);
    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?q=user+query");

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?q=");
  }

  {
    data.created_from_play_api = true;
    TemplateURL t_url(data);

    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?q=user+query"
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?q="
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );
  }
}

TEST_F(TemplateURLTest, GenerateURL_WithEmptyRegulatoryExtensions) {
  TemplateURLData::RegulatoryExtension default_ext{
      .variant = RegulatoryExtensionType::kDefault,
  };
  TemplateURLData::RegulatoryExtension android_eea_ext{
      .variant = RegulatoryExtensionType::kAndroidEEA,
  };
  TemplateURLData data;
  data.SetURL("https://search?q={searchTerms}");
  data.suggestions_url = "https://suggest?q={searchTerms}";
  data.regulatory_extensions.emplace(RegulatoryExtensionType::kDefault,
                                     &default_ext);
  data.regulatory_extensions.emplace(RegulatoryExtensionType::kAndroidEEA,
                                     &android_eea_ext);

  {
    TemplateURL t_url(data);
    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?q=user+query");

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?q=");
  }

  {
    data.created_from_play_api = true;
    TemplateURL t_url(data);

    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?q=user+query"
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?q="
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );
  }
}

TEST_F(TemplateURLTest, GenerateURL_WithFullRegulatoryExtensions) {
  TemplateURLData::RegulatoryExtension default_ext{
      .variant = RegulatoryExtensionType::kDefault,
      .search_params = "default_search_param=123",
      .suggest_params = "default_suggest_param=456"};
  TemplateURLData::RegulatoryExtension android_eea_ext{
      .variant = RegulatoryExtensionType::kAndroidEEA,
      .search_params = "eea_search_param=abc",
      .suggest_params = "eea_suggest_param=def"};

  TemplateURLData data;
  data.SetURL("https://search?q={searchTerms}");
  data.suggestions_url = "https://suggest?q={searchTerms}";
  data.regulatory_extensions.emplace(RegulatoryExtensionType::kDefault,
                                     &default_ext);
  data.regulatory_extensions.emplace(RegulatoryExtensionType::kAndroidEEA,
                                     &android_eea_ext);

  {
    TemplateURL t_url(data);
    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?default_search_param=123&q=user+query");

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?default_suggest_param=456&q=");
  }

  {
    data.created_from_play_api = true;
    TemplateURL t_url(data);

    EXPECT_EQ(t_url.GenerateSearchURL(search_terms_data_, u"user query").spec(),
              "https://search/?eea_search_param=abc&q=user+query"
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );

    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              "https://suggest/?eea_suggest_param=def&q="
#if BUILDFLAG(IS_ANDROID)
              "&chrome_dse_attribution=1"
#endif
    );
  }
}

TEST_F(TemplateURLTest, GenerateURL_RegulatoryExtensionVariantHistograms) {
  TemplateURLData data;
  data.SetURL("https://search?q={searchTerms}");
  data.suggestions_url = "https://suggest?q={searchTerms}";

  {
    TemplateURL t_url(data);
    {
      base::HistogramTester histogram_tester;
      t_url.GenerateSearchURL(search_terms_data_, u"");
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant", 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant",
          RegulatoryExtensionType::kDefault, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant", 0);
    }

    {
      base::HistogramTester histogram_tester;
      t_url.GenerateSuggestionURL(search_terms_data_);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant", 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant",
          RegulatoryExtensionType::kDefault, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant", 0);
    }
  }

  {
    data.created_from_play_api = true;
    TemplateURL t_url(data);

    {
      base::HistogramTester histogram_tester;
      t_url.GenerateSearchURL(search_terms_data_, u"");
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant", 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant",
          RegulatoryExtensionType::kAndroidEEA, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant", 0);
    }

    {
      base::HistogramTester histogram_tester;
      t_url.GenerateSuggestionURL(search_terms_data_);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant", 1);
      histogram_tester.ExpectBucketCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SuggestVariant",
          RegulatoryExtensionType::kAndroidEEA, 1);
      histogram_tester.ExpectTotalCount(
          "Omnibox.TemplateUrl.RegulatoryExtension.SearchVariant", 0);
    }
  }
}

TEST_F(TemplateURLTest, GenerateSuggestionURL) {
  struct GenerateSuggestionURLCase {
    const char* test_name;
    const char* url;
    const char* expected;
  } generate_url_cases[] = {
      {"invalid URL", "foo{searchTerms}", ""},
      {"URL with no replacements", "http://foo/", "http://foo/"},
      {"basic functionality", "http://foo/{searchTerms}", "http://foo/"}};

  for (const auto& generate_url_case : generate_url_cases) {
    TemplateURLData data;
    data.suggestions_url = generate_url_case.url;
    TemplateURL t_url(data);
    EXPECT_EQ(t_url.GenerateSuggestionURL(search_terms_data_).spec(),
              generate_url_case.expected)
        << generate_url_case.test_name << " failed.";
  }
}

TEST_F(TemplateURLTest, PrefetchQueryParameters) {
  TemplateURLData data;
  search_terms_data_.set_google_base_url("http://bar/");
  data.SetURL("http://bar/search?q={searchTerms}&{google:prefetchQuery}xssi=t");

  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
  search_terms_args.prefetch_query = "full query text";
  search_terms_args.prefetch_query_type = "2338";
  std::string result =
      url.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  EXPECT_EQ("http://bar/search?q=foo&pfq=full%20query%20text&qha=2338&xssi=t",
            result);

  TemplateURL url2(data);
  search_terms_args.prefetch_query.clear();
  search_terms_args.prefetch_query_type.clear();
  result =
      url2.url_ref().ReplaceSearchTerms(search_terms_args, search_terms_data_);
  EXPECT_EQ("http://bar/search?q=foo&xssi=t", result);
}

// Tests that TemplateURL works correctly after changing the Google base URL
// and invalidating cached values.
TEST_F(TemplateURLTest, InvalidateCachedValues) {
  TemplateURLData data;
  data.SetURL("{google:baseURL}search?q={searchTerms}");
  data.suggestions_url = "{google:baseSuggestURL}search?q={searchTerms}";
  data.image_url = "{google:baseURL}searchbyimage/upload";
  data.image_translate_url = "{google:baseURL}searchbyimage/upload?translate";
  data.new_tab_url = "{google:baseURL}_/chrome/newtab";
  data.contextual_search_url = "{google:baseURL}_/contextualsearch";
  data.alternate_urls.push_back("{google:baseURL}s#q={searchTerms}");
  TemplateURL url(data);
  TemplateURLRef::SearchTermsArgs search_terms_args(u"X");
  std::u16string search_terms;

  EXPECT_TRUE(url.HasGoogleBaseURLs(search_terms_data_));
  EXPECT_EQ("http://www.google.com/search?q=X",
            url.url_ref().ReplaceSearchTerms(search_terms_args,
                                             search_terms_data_));
  EXPECT_EQ("http://www.google.com/s#q=X",
            url.url_refs()[0].ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
  EXPECT_EQ("http://www.google.com/search?q=X",
            url.url_refs()[1].ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
  EXPECT_EQ("http://www.google.com/complete/search?q=X",
            url.suggestions_url_ref().ReplaceSearchTerms(search_terms_args,
                                                         search_terms_data_));
  EXPECT_EQ("http://www.google.com/searchbyimage/upload",
            url.image_url_ref().ReplaceSearchTerms(search_terms_args,
                                                   search_terms_data_));
  EXPECT_EQ("http://www.google.com/searchbyimage/upload?translate",
            url.image_translate_url_ref().ReplaceSearchTerms(
                search_terms_args, search_terms_data_));
  EXPECT_EQ("http://www.google.com/_/chrome/newtab",
            url.new_tab_url_ref().ReplaceSearchTerms(search_terms_args,
                                                     search_terms_data_));
  EXPECT_EQ("http://www.google.com/_/contextualsearch",
            url.contextual_search_url_ref().ReplaceSearchTerms(
                search_terms_args, search_terms_data_));

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://www.google.com/search?q=Y+Z"),
      search_terms_data_, &search_terms));
  EXPECT_EQ(u"Y Z", search_terms);
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("http://www.google.com/s#q=123"),
      search_terms_data_, &search_terms));
  EXPECT_EQ(u"123", search_terms);

  search_terms_data_.set_google_base_url("https://www.foo.org/");
  url.InvalidateCachedValues();

  EXPECT_TRUE(url.HasGoogleBaseURLs(search_terms_data_));
  EXPECT_EQ("https://www.foo.org/search?q=X",
            url.url_ref().ReplaceSearchTerms(search_terms_args,
                                             search_terms_data_));
  EXPECT_EQ("https://www.foo.org/s#q=X",
            url.url_refs()[0].ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
  EXPECT_EQ("https://www.foo.org/search?q=X",
            url.url_refs()[1].ReplaceSearchTerms(search_terms_args,
                                                 search_terms_data_));
  EXPECT_EQ("https://www.foo.org/complete/search?q=X",
            url.suggestions_url_ref().ReplaceSearchTerms(search_terms_args,
                                                         search_terms_data_));
  EXPECT_EQ("https://www.foo.org/searchbyimage/upload",
            url.image_url_ref().ReplaceSearchTerms(search_terms_args,
                                                   search_terms_data_));
  EXPECT_EQ("https://www.foo.org/searchbyimage/upload?translate",
            url.image_translate_url_ref().ReplaceSearchTerms(
                search_terms_args, search_terms_data_));
  EXPECT_EQ("https://www.foo.org/_/chrome/newtab",
            url.new_tab_url_ref().ReplaceSearchTerms(search_terms_args,
                                                     search_terms_data_));
  EXPECT_EQ("https://www.foo.org/_/contextualsearch",
            url.contextual_search_url_ref().ReplaceSearchTerms(
                search_terms_args, search_terms_data_));

  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("https://www.foo.org/search?q=Y+Z"),
      search_terms_data_, &search_terms));
  EXPECT_EQ(u"Y Z", search_terms);
  EXPECT_TRUE(url.ExtractSearchTermsFromURL(
      GURL("https://www.foo.org/s#q=123"),
      search_terms_data_, &search_terms));
  EXPECT_EQ(u"123", search_terms);

  search_terms_data_.set_google_base_url("http://www.google.com/");
}

// Tests the use of wildcards in the path to ensure both extracting search terms
// and generating a search URL work correctly.
TEST_F(TemplateURLTest, PathWildcard) {
  TemplateURLData data;
  data.SetURL(
      "https://www.google.com/search{google:pathWildcard}?q={searchTerms}");
  TemplateURL url(data);

  // Test extracting search terms from a URL.
  std::u16string search_terms;
  url.ExtractSearchTermsFromURL(GURL("https://www.google.com/search?q=testing"),
                                search_terms_data_, &search_terms);
  EXPECT_EQ(u"testing", search_terms);
  url.ExtractSearchTermsFromURL(
      GURL("https://www.google.com/search;_this_is_a_test;_?q=testing"),
      search_terms_data_, &search_terms);
  EXPECT_EQ(u"testing", search_terms);

  // Tests overlapping prefix/suffix.
  data.SetURL(
      "https://www.google.com/search{google:pathWildcard}rch?q={searchTerms}");
  TemplateURL overlap_url(data);
  overlap_url.ExtractSearchTermsFromURL(
      GURL("https://www.google.com/search?q=testing"), search_terms_data_,
      &search_terms);
  EXPECT_TRUE(search_terms.empty());

  // Tests wildcard at beginning of path so we only have a suffix.
  data.SetURL(
      "https://www.google.com/{google:pathWildcard}rch?q={searchTerms}");
  TemplateURL suffix_url(data);
  suffix_url.ExtractSearchTermsFromURL(
      GURL("https://www.google.com/search?q=testing"), search_terms_data_,
      &search_terms);
  EXPECT_EQ(u"testing", search_terms);

  // Tests wildcard between prefix/suffix.
  overlap_url.ExtractSearchTermsFromURL(
      GURL("https://www.google.com/search_testing_rch?q=testing"),
      search_terms_data_, &search_terms);
  EXPECT_EQ(u"testing", search_terms);

  // Test generating a URL.
  TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
  GURL generated_url;
  url.ReplaceSearchTermsInURL(url.GenerateSearchURL(search_terms_data_),
                              search_terms_args, search_terms_data_,
                              &generated_url);
  EXPECT_EQ("https://www.google.com/search?q=foo", generated_url.spec());
}

TEST_F(TemplateURLTest, SideImageSearchParams) {
  TemplateURLData data;
  data.side_image_search_param = "sideimagesearch";
  TemplateURL url(data);

  // Adds query param with provided version to URL.
  GURL result =
      url.GenerateSideImageSearchURL(GURL("http://foo.com/?q=123"), "1");
  EXPECT_EQ("http://foo.com/?q=123&sideimagesearch=1", result.spec());

  // Does not add query param if the provided URL already has that param and
  // version.
  result = url.GenerateSideImageSearchURL(
      GURL("http://foo.com/?q=123&sideimagesearch=1"), "1");
  EXPECT_EQ("http://foo.com/?q=123&sideimagesearch=1", result.spec());

  // Updates version if the version on the query param does not match.
  result = url.GenerateSideImageSearchURL(
      GURL("http://foo.com/?q=123&sideimagesearch=2"), "1");
  EXPECT_EQ("http://foo.com/?q=123&sideimagesearch=1", result.spec());

  // Does nothing if the URL does not have the param.
  result = url.RemoveSideImageSearchParamFromURL(GURL("http://foo.com/?q=123"));
  EXPECT_EQ("http://foo.com/?q=123", result.spec());

  // Removes the param if the provided URL has it.
  result = url.RemoveSideImageSearchParamFromURL(
      GURL("http://foo.com/?q=123&sideimagesearch=1"));
  EXPECT_EQ("http://foo.com/?q=123", result.spec());

  // Removes the first instance of the query param that exist in the URL. This
  // should not happen but just asserting for expected behavior.
  result = url.RemoveSideImageSearchParamFromURL(
      GURL("http://foo.com/?q=123&sideimagesearch=1&sideimagesearch=2"));
  EXPECT_EQ("http://foo.com/?q=123&sideimagesearch=2", result.spec());
}

TEST_F(TemplateURLTest, ImageTranslate) {
  struct TestData {
    const std::string image_translate_url;
    const std::string image_translate_source_language_param_key;
    const std::string image_translate_target_language_param_key;
    const std::string image_translate_source_locale;
    const std::string image_translate_target_locale;
    const std::string expected_result;
  } test_data[] = {
      {"https://lens.google.com/upload?filtertype=translate"
       "&{imageTranslateSourceLocale}{imageTranslateTargetLocale}",
       "sourcelang", "targetlang", "zh-CN", "fr",
       "https://lens.google.com/"
       "upload?filtertype=translate&sourcelang=zh-CN&targetlang=fr&"},
      {"https://www.foo.com/images/detail/search"
       "?ft=tr&{imageTranslateSourceLocale}{imageTranslateTargetLocale}"
       "#fragment",
       "sl", "tl", "ja", "es",
       "https://www.foo.com/images/detail/search?ft=tr&sl=ja&tl=es&#fragment"},
      {"https://bar.com/images/translate/"
       "{imageTranslateSourceLocale}/{imageTranslateTargetLocale}/",
       "", "", "ko", "de", "https://bar.com/images/translate/ko/de/"},
  };
  TemplateURLData data;
  for (const auto& entry : test_data) {
    data.image_translate_url = entry.image_translate_url;
    data.image_translate_source_language_param_key =
        entry.image_translate_source_language_param_key;
    data.image_translate_target_language_param_key =
        entry.image_translate_target_language_param_key;
    TemplateURL url(data);
    TemplateURLRef::SearchTermsArgs search_terms_args(u"");
    search_terms_args.image_translate_source_locale =
        entry.image_translate_source_locale;
    search_terms_args.image_translate_target_locale =
        entry.image_translate_target_locale;
    GURL result(url.image_translate_url_ref().ReplaceSearchTerms(
        search_terms_args, search_terms_data_));
    ASSERT_TRUE(result.is_valid());
    EXPECT_EQ(entry.expected_result, result.spec());
  }
}

TEST_F(TemplateURLTest, ImageSearchBrandingLabel) {
  TemplateURLData data;
  data.SetShortName(u"foo");
  TemplateURL no_image_branding_url(data);

  // Without an image_search_branding_label set, should return short_name
  EXPECT_EQ(u"foo", no_image_branding_url.image_search_branding_label());

  data.image_search_branding_label = u"fooimages";
  TemplateURL image_branding_url(data);
  EXPECT_EQ(u"fooimages", image_branding_url.image_search_branding_label());
}

struct IsBetterThanEngineTestEngine {
  std::u16string keyword;
  CreatedByPolicy created_by_policy = CreatedByPolicy::kNoPolicy;
  bool enforced_by_policy = false;
  bool featured_by_policy = false;
  bool safe_for_autoreplace = false;
  base::Time last_modified;
};

TemplateURL CreateEngineFromTestEngine(
    const IsBetterThanEngineTestEngine& engine) {
  TemplateURLData template_url_data;
  template_url_data.SetKeyword(engine.keyword);
  template_url_data.SetShortName(engine.keyword + u"_name");
  template_url_data.SetURL("https://" + base::UTF16ToUTF8(engine.keyword) +
                           ".com/q={searchTerms}");
  template_url_data.created_by_policy = engine.created_by_policy;
  template_url_data.enforced_by_policy = engine.enforced_by_policy;
  template_url_data.featured_by_policy = engine.featured_by_policy;
  template_url_data.safe_for_autoreplace = engine.safe_for_autoreplace;
  template_url_data.last_modified = engine.last_modified;
  return TemplateURL(template_url_data);
}

struct IsBetterThanEngineTestCase {
  std::string description;
  IsBetterThanEngineTestEngine better_engine;
  IsBetterThanEngineTestEngine worse_engine;
} kTestCases[] = {
    {
        .description = "NonFeaturedByPolicy_SafeForAutoreplace",
        .better_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .safe_for_autoreplace = true,
            },
    },
    {
        .description = "NonFeaturedByPolicy_EditedByUser",
        .better_engine =
            {
                .keyword = u"kw",
                .safe_for_autoreplace = false,
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
            },
    },
    {
        .description = "FeaturedByPolicy_EditedByUser",
        .better_engine =
            {
                .keyword = u"@kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
                .featured_by_policy = true,
            },
        .worse_engine =
            {
                .keyword = u"@kw",
                .safe_for_autoreplace = false,
            },
    },
    {
        .description = "FeaturedByPolicy_NonFeaturedByPolicy",
        .better_engine =
            {
                .keyword = u"@kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
                .featured_by_policy = true,
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
            },
    },
    {
        .description = "DefaultSearchProvider_MandatoryPolicy_UserDefined",
        .better_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kDefaultSearchProvider,
                .enforced_by_policy = true,
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kNoPolicy,
                .safe_for_autoreplace = false,
            },
    },
    {
        .description = "DefaultSearchProvider_MandatoryPolicy_Recommended",
        .better_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kDefaultSearchProvider,
                .enforced_by_policy = true,
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kDefaultSearchProvider,
            },
    },
    {
        .description = "SiteSearchPolicy_Fallback",
        .better_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
                .last_modified = base::Time::FromTimeT(2000),
            },
        .worse_engine =
            {
                .keyword = u"kw",
                .created_by_policy = CreatedByPolicy::kSiteSearch,
                .last_modified = base::Time::FromTimeT(1000),
            },
    },
};

class TemplateURLIsBetterThanEngineTest
    : public TemplateURLTest,
      public testing::WithParamInterface<IsBetterThanEngineTestCase> {};

std::string ParamToTestSuffix(
    const ::testing::TestParamInfo<IsBetterThanEngineTestCase>& info) {
  return info.param.description;
}

INSTANTIATE_TEST_SUITE_P(,
                         TemplateURLIsBetterThanEngineTest,
                         ::testing::ValuesIn(kTestCases),
                         &ParamToTestSuffix);

TEST_P(TemplateURLIsBetterThanEngineTest, Compare) {
  TemplateURL better = CreateEngineFromTestEngine(GetParam().better_engine);
  TemplateURL worse = CreateEngineFromTestEngine(GetParam().worse_engine);
  EXPECT_TRUE(better.IsBetterThanConflictingEngine(&worse));
  EXPECT_FALSE(worse.IsBetterThanConflictingEngine(&better));
}
