// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/google/browser/google_url_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_load_metrics {

class PageLoadMetricsUtilTest : public testing::Test {};

TEST_F(PageLoadMetricsUtilTest, IsGoogleHostname) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://google.com/"},
      {true, "https://google.com/index.html"},
      {true, "https://www.google.com/"},
      {true, "https://www.google.com/search"},
      {true, "https://www.google.com/a/b/c/d"},
      {true, "https://www.google.co.uk/"},
      {true, "https://www.google.co.in/"},
      {true, "https://other.google.com/"},
      {true, "https://other.www.google.com/"},
      {true, "https://www.other.google.com/"},
      {true, "https://www.www.google.com/"},
      {false, ""},
      {false, "a"},
      {false, "*"},
      {false, "com"},
      {false, "co.uk"},
      {false, "google"},
      {false, "google.com"},
      {false, "www.google.com"},
      {false, "https:///"},
      {false, "https://a/"},
      {false, "https://*/"},
      {false, "https://com/"},
      {false, "https://co.uk/"},
      {false, "https://google/"},
      {false, "https://*.com/"},
      {false, "https://www.*.com/"},
      {false, "https://www.google.appspot.com/"},
      {false, "https://www.google.example.com/"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleHostname(GURL(test.url)))
        << "For URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, GetGoogleHostnamePrefix) {
  struct {
    bool expected_result;
    const char* expected_prefix;
    const char* url;
  } test_cases[] = {
      {false, "", "https://example.com/"},
      {true, "", "https://google.com/"},
      {true, "www", "https://www.google.com/"},
      {true, "news", "https://news.google.com/"},
      {true, "www", "https://www.google.co.uk/"},
      {true, "other", "https://other.google.com/"},
      {true, "other.www", "https://other.www.google.com/"},
      {true, "www.other", "https://www.other.google.com/"},
      {true, "www.www", "https://www.www.google.com/"},
  };
  for (const auto& test : test_cases) {
    std::optional<std::string> result =
        page_load_metrics::GetGoogleHostnamePrefix(GURL(test.url));
    EXPECT_EQ(test.expected_result, result.has_value())
        << "For URL: " << test.url;
    if (result) {
      EXPECT_EQ(test.expected_prefix, result.value())
          << "Prefix for URL: " << test.url;
    }
  }
}

TEST_F(PageLoadMetricsUtilTest, HasGoogleSearchQuery) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/search#q=test"},
      {true, "https://www.google.com/search?q=test"},
      {true, "https://www.google.com#q=test"},
      {true, "https://www.google.com?q=test"},
      {false, "https://www.google.com/search"},
      {false, "https://www.google.com/"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::HasGoogleSearchQuery(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, IsGoogleSearchHostname) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/"},
      {true, "https://www.google.co.uk/"},
      {true, "https://www.google.co.in/"},
      {false, "https://other.google.com/"},
      {false, "https://other.www.google.com/"},
      {false, "https://www.other.google.com/"},
      {false, "https://www.www.google.com/"},
      {false, "https://www.google.appspot.com/"},
      {false, "https://www.google.example.com/"},
      // Search results are not served from the bare google.com domain.
      {false, "https://google.com/"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleSearchHostname(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, IsProbablyGoogleSearchUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {false, "http://www.example.com/"},
      {false, "https://google.com/#q=test"},
      {false, "https://google.com/url?"},
      {false, "https://other.google.com/"},
      {false, "https://other.google.com/webhp?q=test"},
      {false, "https://www.example.com/url?source=web"},
      {false, "https://www.example.com/url?source=web"},
      {false, "https://www.example.com/webhp?q=test"},
      {true, "https://www.google.com/?"},
      {true, "https://www.google.com/?source=web"},
      {true, "https://www.google.com/?url"},
      {true, "https://www.google.com/"},
      {true, "https://www.google.com/#q=test"},
      {true, "https://www.google.com/about/"},
      {true, "https://www.google.com/search?q=test"},
      {true, "https://www.google.com/search#q=test"},
      {true, "https://www.google.com/searchurl/r.html#foo"},
      {true, "https://www.google.com/source=web"},
      {true, "https://www.google.com/url?"},
      {true, "https://www.google.com/url?a=b"},
      {true, "https://www.google.com/url?a=b&source=web&c=d"},
      {true, "https://www.google.com/url?source=web"},
      {true, "https://www.google.com/url?source=web#foo"},
      {true, "https://www.google.com/webhp?#a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp?a=b&q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp?q=test"},
      {true, "https://www.google.com/webhp#a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp#q=test"},
      {true, "https://www.google.com/webmasters/#?modal_active=none"},
      {false, "https://www.google.com/maps"},
      {false,
       "https://www.google.com/maps/place/Shibuya+Stream/"
       "@35.6572693,139.7031288,15z/"
       "data=!4m2!3m1!1s0x0:0x387c407b91e2ad68?sa=X&ved=1t:2428&ictx=111"},
      {false,
       "https://www.google.com/maps/reviews/"
       "data=!4m5!14m4!1m3!1m2!1s102657011957627300761!2s0x60188b31a00165ed:"
       "0x387c407b91e2ad68?ved=1t:31295&ictx=111"},
      {false, "https://www.google.co.jp/maps"},
      {false,
       "https://www.google.co.jp/maps/place/Shibuya+Stream/"
       "@35.6572693,139.7031288,15z/"
       "data=!4m2!3m1!1s0x0:0x387c407b91e2ad68?sa=X&ved=1t:2428&ictx=111"},
      {false,
       "https://www.google.co.jp/maps/reviews/"
       "data=!4m5!14m4!1m3!1m2!1s102657011957627300761!2s0x60188b31a00165ed:"
       "0x387c407b91e2ad68?ved=1t:31295&ictx=111"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsProbablyGoogleSearchUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, IsGoogleSearchResultUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/#q=test"},
      {true, "https://www.google.com/search#q=test"},
      {true, "https://www.google.com/search?q=test"},
      {true, "https://www.google.com/webhp#q=test"},
      {true, "https://www.google.com/webhp?q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp#a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp?#a=b&q=test&c=d"},
      {false, "https://www.google.com/"},
      {false, "https://www.google.com/about/"},
      {false, "https://other.google.com/"},
      {false, "https://other.google.com/webhp?q=test"},
      {false, "http://www.example.com/"},
      {false, "https://www.example.com/webhp?q=test"},
      {false, "https://google.com/#q=test"},
      // Regression test for crbug.com/805155
      {false, "https://www.google.com/webmasters/#?modal_active=none"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleSearchResultUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, IsGoogleSearchHomepageUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {false, "https://www.google.com/search#q=test"},
      {false, "https://www.google.com/search?q=test"},
      {false, "https://www.google.com/custom?q=test"},
      {true, "https://www.google.com/search"},
      {true, "https://www.google.com/custom"},
      {true, "https://www.google.com/#q=test"},
      {true, "https://www.google.com/webhp#q=test"},
      {true, "https://www.google.com/webhp?q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test"},
      {true, "https://www.google.com/webhp?a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp#a=b&q=test&c=d"},
      {true, "https://www.google.com/webhp?#a=b&q=test&c=d"},
      {true, "https://www.google.com/"},
      {false, "https://www.google.com/about/"},
      {false, "https://other.google.com/"},
      {false, "https://other.google.com/webhp?q=test"},
      {false, "http://www.example.com/"},
      {false, "https://www.example.com/webhp?q=test"},
      {false, "https://google.com/#q=test"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleSearchHomepageUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

TEST_F(PageLoadMetricsUtilTest, IsGoogleSearchRedirectorUrl) {
  struct {
    bool expected_result;
    const char* url;
  } test_cases[] = {
      {true, "https://www.google.com/url?source=web"},
      {true, "https://www.google.com/url?source=web#foo"},
      {true, "https://www.google.com/searchurl/r.html#foo"},
      {true, "https://www.google.com/url?a=b&source=web&c=d"},
      {false, "https://www.google.com/?"},
      {false, "https://www.google.com/?url"},
      {false, "https://www.example.com/url?source=web"},
      {false, "https://google.com/url?"},
      {false, "https://www.google.com/?source=web"},
      {false, "https://www.google.com/source=web"},
      {false, "https://www.example.com/url?source=web"},
      {false, "https://www.google.com/url?"},
      {false, "https://www.google.com/url?a=b"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::IsGoogleSearchRedirectorUrl(GURL(test.url)))
        << "for URL: " << test.url;
  }
}

}  // namespace page_load_metrics
