// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/safe_search_util.h"

#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
// Does a request using the |url_string| URL and verifies that the expected
// string is equal to the query part (between ? and #) of the final url of
// that request.
void CheckAddedParameters(const std::string& url_string,
                          const std::string& expected_query_parameters) {
  // Show the URL in the trace so we know where we failed.
  SCOPED_TRACE(url_string);

  GURL result(url_string);
  safe_search_api::ForceGoogleSafeSearch(GURL(url_string), &result);

  EXPECT_EQ(expected_query_parameters, result.query());
}

TEST(SafeSearchUtilTest, AddGoogleSafeSearchParams) {
  const std::string kSafeParameter = safe_search_api::kSafeSearchSafeParameter;
  const std::string kSsuiParameter = safe_search_api::kSafeSearchSsuiParameter;
  const std::string kBothParameters = kSafeParameter + "&" + kSsuiParameter;

  // Test the home page.
  CheckAddedParameters("http://google.com/", kBothParameters);

  // Test the search home page.
  CheckAddedParameters("http://google.com/webhp", kBothParameters);

  // Test different valid search pages with parameters.
  CheckAddedParameters("http://google.com/search?q=google",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/?q=google",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google",
                       "q=google&" + kBothParameters);

  // Test the valid pages with safe set to off.
  CheckAddedParameters("http://google.com/search?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google&safe=off",
                       "q=google&" + kBothParameters);

  CheckAddedParameters("http://google.com/webhp?q=google&%73afe=off",
                       "q=google&%73afe=off&" + kBothParameters);

  // Test the home page, different TLDs.
  CheckAddedParameters("http://google.de/", kBothParameters);
  CheckAddedParameters("http://google.ro/", kBothParameters);
  CheckAddedParameters("http://google.nl/", kBothParameters);

  // Test the search home page, different TLD.
  CheckAddedParameters("http://google.de/webhp", kBothParameters);

  // Test the search page with parameters, different TLD.
  CheckAddedParameters("http://google.de/search?q=google",
                       "q=google&" + kBothParameters);

  // Test the home page with parameters, different TLD.
  CheckAddedParameters("http://google.de/?q=google",
                       "q=google&" + kBothParameters);

  // Test the search page with the parameters set.
  CheckAddedParameters("http://google.de/?q=google&" + kBothParameters,
                       "q=google&" + kBothParameters);

  // Test some possibly tricky combinations.
  CheckAddedParameters(
      "http://google.com/?q=goog&" + kSafeParameter + "&ssui=one",
      "q=goog&" + kBothParameters);

  CheckAddedParameters(
      "http://google.de/?q=goog&unsafe=active&" + kSsuiParameter,
      "q=goog&unsafe=active&" + kBothParameters);

  CheckAddedParameters("http://google.de/?q=goog&safe=off&ssui=off",
                       "q=goog&" + kBothParameters);

  CheckAddedParameters("http://google.de/?q=&tbs=rimg:",
                       "q=&tbs=rimg:&" + kBothParameters);

  // Test various combinations where we should not add anything.
  CheckAddedParameters(
      "http://google.com/?q=goog&" + kSsuiParameter + "&" + kSafeParameter,
      "q=goog&" + kBothParameters);

  CheckAddedParameters(
      "http://google.com/?" + kSsuiParameter + "&q=goog&" + kSafeParameter,
      "q=goog&" + kBothParameters);

  CheckAddedParameters(
      "http://google.com/?" + kSsuiParameter + "&" + kSafeParameter + "&q=goog",
      "q=goog&" + kBothParameters);

  // Test that another website is not affected, without parameters.
  CheckAddedParameters("http://google.com/finance", std::string());

  // Test that another website is not affected, with parameters.
  CheckAddedParameters("http://google.com/finance?q=goog", "q=goog");

  // Test with percent-encoded data (%26 is &)
  CheckAddedParameters("http://google.com/?q=%26%26%26&" + kSsuiParameter +
                           "&" + kSafeParameter + "&param=%26%26%26",
                       "q=%26%26%26&param=%26%26%26&" + kBothParameters);

  // Test with image search
  CheckAddedParameters("http://google.com/imgres?imgurl=https://image",
                       "imgurl=https://image&" + kBothParameters);
}

TEST(SafeSearchUtilTest, SafeSearchSettingsPage) {
  const std::string kBothParameters =
      std::string(safe_search_api::kSafeSearchSafeParameter) + "&" +
      safe_search_api::kSafeSearchSsuiParameter;

  CheckAddedParameters("https://www.google.com/safesearch", kBothParameters);
  CheckAddedParameters("https://google.ca/safesearch", kBothParameters);
  CheckAddedParameters("https://ipv4.google.com/safesearch", kBothParameters);
  CheckAddedParameters("https://ipv4.google.com/safesearch?safe=off",
                       kBothParameters);
}

TEST(SafeSearchUtilTest, SetYoutubeHeader) {
  net::HttpRequestHeaders headers;
  safe_search_api::ForceYouTubeRestrict(
      GURL("http://www.youtube.com"), &headers,
      safe_search_api::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_THAT(headers.GetHeader("Youtube-Restrict"),
              testing::Optional(std::string("Moderate")));
}

TEST(SafeSearchUtilTest, OverrideYoutubeHeader) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Youtube-Restrict", "Off");
  safe_search_api::ForceYouTubeRestrict(
      GURL("http://www.youtube.com"), &headers,
      safe_search_api::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_THAT(headers.GetHeader("Youtube-Restrict"),
              testing::Optional(std::string("Moderate")));
}

TEST(SafeSearchUtilTest, DoesntTouchNonYoutubeURL) {
  net::HttpRequestHeaders headers;
  headers.SetHeader("Youtube-Restrict", "Off");
  safe_search_api::ForceYouTubeRestrict(
      GURL("http://www.notyoutube.com"), &headers,
      safe_search_api::YOUTUBE_RESTRICT_MODERATE);
  EXPECT_THAT(headers.GetHeader("Youtube-Restrict"),
              testing::Optional(std::string("Off")));
}

}  // namespace
