// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_util.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/page_load_metrics/browser/fake_page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
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

TEST_F(PageLoadMetricsUtilTest, QueryContainsComponent) {
  struct {
    bool expected_result;
    const char* query;
    const char* component;
  } test_cases[] = {
      {true, "a=b", "a=b"},
      {true, "a=b&c=d", "a=b"},
      {true, "a=b&c=d", "c=d"},
      {true, "a=b&c=d&e=f", "c=d"},
      {true, "za=b&a=b", "a=b"},
      {true, "a=bz&a=b", "a=b"},
      {true, "a=ba=b&a=b", "a=b"},
      {true, "a=a=a&a=a", "a=a"},
      {true, "source=web", "source=web"},
      {true, "a=b&source=web", "source=web"},
      {true, "a=b&source=web&c=d", "source=web"},
      {false, "a=a=a", "a=a"},
      {false, "", ""},
      {false, "a=b", ""},
      {false, "", "a=b"},
      {false, "za=b", "a=b"},
      {false, "za=bz", "a=b"},
      {false, "a=bz", "a=b"},
      {false, "za=b&c=d", "a=b"},
      {false, "a=b&c=dz", "c=d"},
      {false, "a=b&zc=d&e=f", "c=d"},
      {false, "a=b&c=dz&e=f", "c=d"},
      {false, "a=b&zc=dz&e=f", "c=d"},
      {false, "a=b&foosource=web&c=d", "source=web"},
      {false, "a=b&source=webbar&c=d", "source=web"},
      {false, "a=b&foosource=webbar&c=d", "source=web"},
      // Correctly handle cases where there is a leading "?" or "#" character.
      {true, "?a=b&source=web", "a=b"},
      {false, "a=b&?source=web", "source=web"},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result, page_load_metrics::QueryContainsComponent(
                                        test.query, test.component))
        << "For query: " << test.query << " with component: " << test.component;
  }
}

TEST_F(PageLoadMetricsUtilTest, QueryContainsComponentPrefix) {
  struct {
    bool expected_result;
    const char* query;
    const char* component;
  } test_cases[] = {
      {true, "a=b", "a="},
      {true, "a=b&c=d", "a="},
      {true, "a=b&c=d", "c="},
      {true, "a=b&c=d&e=f", "c="},
      {true, "za=b&a=b", "a="},
      {true, "ba=a=b&a=b", "a="},
      {true, "q=test", "q="},
      {true, "a=b&q=test", "q="},
      {true, "q=test&c=d", "q="},
      {true, "a=b&q=test&c=d", "q="},
      {false, "", ""},
      {false, "za=b", "a="},
      {false, "za=b&c=d", "a="},
      {false, "a=b&zc=d", "c="},
      {false, "a=b&zc=d&e=f", "c="},
      {false, "a=b&zq=test&c=d", "q="},
      {false, "ba=a=b", "a="},
  };
  for (const auto& test : test_cases) {
    EXPECT_EQ(test.expected_result,
              page_load_metrics::QueryContainsComponentPrefix(test.query,
                                                              test.component))
        << "For query: " << test.query << " with component: " << test.component;
  }
}

TEST_F(PageLoadMetricsUtilTest, UmaMaxCumulativeShiftScoreHistogram) {
  constexpr char kTestMaxCumulativeShiftScoreSessionWindow[] = "Test";
  const page_load_metrics::NormalizedCLSData normalized_cls_data{0.5, false};
  base::HistogramTester histogram_tester;
  page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
      kTestMaxCumulativeShiftScoreSessionWindow, normalized_cls_data);
  histogram_tester.ExpectTotalCount(kTestMaxCumulativeShiftScoreSessionWindow,
                                    1);
  histogram_tester.ExpectBucketCount(kTestMaxCumulativeShiftScoreSessionWindow,
                                     5000, 1);
}

TEST_F(PageLoadMetricsUtilTest, GetNonPrerenderingBackgroundStartTiming) {
  struct {
    PrerenderingState prerendering_state;
    std::optional<base::TimeDelta> activation_start;
    PageVisibility visibility_at_start_or_activation_;
    std::optional<base::TimeDelta> time_to_first_background;
    std::optional<base::TimeDelta> expected_result;
  } test_cases[] = {
      {PrerenderingState::kNoPrerendering, std::nullopt,
       PageVisibility::kForeground, std::nullopt, std::nullopt},
      {PrerenderingState::kNoPrerendering, std::nullopt,
       PageVisibility::kForeground, base::Seconds(2), base::Seconds(2)},
      {PrerenderingState::kNoPrerendering, std::nullopt,
       PageVisibility::kBackground, std::nullopt, base::Seconds(0)},
      {PrerenderingState::kNoPrerendering, std::nullopt,
       PageVisibility::kBackground, base::Seconds(2), base::Seconds(0)},
      {PrerenderingState::kInPrerendering, std::nullopt,
       PageVisibility::kForeground, std::nullopt, std::nullopt},
      {PrerenderingState::kInPrerendering, std::nullopt,
       PageVisibility::kForeground, base::Seconds(10), std::nullopt},
      {PrerenderingState::kActivatedNoActivationStart, std::nullopt,
       PageVisibility::kForeground, base::Seconds(12), std::nullopt},
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kForeground, std::nullopt, std::nullopt},
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kForeground, base::Seconds(12), base::Seconds(12)},
      // Invalid time_to_first_background. Not checked and may return invalid
      // value.
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kForeground, base::Seconds(2), base::Seconds(2)},
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kBackground, std::nullopt, base::Seconds(10)},
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kBackground, base::Seconds(12), base::Seconds(10)},
      // Invalid time_to_first_background. Not checked and may return invalid
      // value.
      {PrerenderingState::kActivated, base::Seconds(10),
       PageVisibility::kBackground, base::Seconds(2), base::Seconds(10)},
  };
  for (const auto& test_case : test_cases) {
    page_load_metrics::FakePageLoadMetricsObserverDelegate delegate;
    delegate.prerendering_state_ = test_case.prerendering_state;
    delegate.activation_start_ = test_case.activation_start;
    if (test_case.time_to_first_background.has_value()) {
      delegate.first_background_time_ =
          delegate.navigation_start_ +
          test_case.time_to_first_background.value();
    } else {
      delegate.first_background_time_ = std::nullopt;
    }

    switch (test_case.prerendering_state) {
      case PrerenderingState::kNoPrerendering:
      case PrerenderingState::kInPreview:
        DCHECK_NE(test_case.visibility_at_start_or_activation_,
                  PageVisibility::kNotInitialized);
        delegate.started_in_foreground_ =
            (test_case.visibility_at_start_or_activation_ ==
             PageVisibility::kForeground);
        delegate.visibility_at_activation_ = PageVisibility::kNotInitialized;
        break;
      case PrerenderingState::kInPrerendering:
        delegate.started_in_foreground_ = false;
        delegate.visibility_at_activation_ = PageVisibility::kNotInitialized;
        break;
      case PrerenderingState::kActivatedNoActivationStart:
        delegate.started_in_foreground_ = false;
        delegate.visibility_at_activation_ =
            test_case.visibility_at_start_or_activation_;
        break;
      case PrerenderingState::kActivated:
        delegate.started_in_foreground_ = false;
        delegate.visibility_at_activation_ =
            test_case.visibility_at_start_or_activation_;
        break;
    }

    std::optional<base::TimeDelta> got =
        GetNonPrerenderingBackgroundStartTiming(delegate);
    EXPECT_EQ(test_case.expected_result, got);
  }
}

TEST_F(PageLoadMetricsUtilTest, CorrectEventAsNavigationOrActivationOrigined) {
  struct {
    PrerenderingState prerendering_state;
    std::optional<base::TimeDelta> activation_start;
    base::TimeDelta event;
    std::optional<base::TimeDelta> expected_result;
  } test_cases[] = {
      // Not modified
      {PrerenderingState::kNoPrerendering, std::nullopt, base::Seconds(2),
       base::Seconds(2)},
      // max(0, 2 - x), where x is time of activation start that may come in the
      // future and should be greater than an already occurred event.
      {PrerenderingState::kInPrerendering, std::nullopt, base::Seconds(2),
       base::Seconds(0)},
      {PrerenderingState::kActivatedNoActivationStart, std::nullopt,
       base::Seconds(2), base::Seconds(0)},
      // crash due to incorrect data
      {PrerenderingState::kActivated, base::Seconds(10), base::Seconds(2),
       base::Seconds(-1)},
      // max(0, 12 - 10)
      {PrerenderingState::kActivated, base::Seconds(10), base::Seconds(12),
       base::Seconds(2)},
  };

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  for (const auto& test_case : test_cases) {
    page_load_metrics::FakePageLoadMetricsObserverDelegate delegate;
    delegate.prerendering_state_ = test_case.prerendering_state;
    delegate.activation_start_ = test_case.activation_start;

    auto test_expectation_runner =
        [&](base::TimeDelta event,
            std::optional<base::TimeDelta> expected_result) {
          if (expected_result->is_negative()) {
            EXPECT_DEATH_IF_SUPPORTED(
                CorrectEventAsNavigationOrActivationOrigined(delegate, timing,
                                                             event),
                "");
          } else {
            base::TimeDelta got = CorrectEventAsNavigationOrActivationOrigined(
                delegate, timing, event);
            EXPECT_EQ(expected_result, got);
          }
        };

    test_expectation_runner(test_case.event, test_case.expected_result);

    // Currently, multiple implementations of PageLoadMetricsObserver is
    // ongoing. We'll left the old version for a while.
    // TODO(crbug.com/40222513): Delete below.
    timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
    timing.activation_start = test_case.activation_start;
    test_expectation_runner(test_case.event, test_case.expected_result);

    // In some path, this function is called with old PageLoadTiming, which can
    // lack activation_start. The result is the same for such case.
    timing.activation_start = std::nullopt;
    test_expectation_runner(test_case.event, test_case.expected_result);
  }
}

}  // namespace page_load_metrics
