// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/navigation_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char* const kTestUrl = "http://www.example.com";
// http://ab.גדהוזח.ij/kl/mn/op.html in A-label form.
constexpr char kRtlUrl[] = "http://ab.xn--6dbcdefg.ij/kl/mn/op.html";
}  // namespace

namespace navigation_metrics {

TEST(NavigationMetrics, MainFrameSchemeDifferentDocument) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
}

TEST(NavigationMetrics, MainFrameSchemeDifferentDocument_NonUniqueHostname) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL("http://site.test"), false, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPageNonUniqueHostname,
                          1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
}

TEST(NavigationMetrics, MainFrameSchemeSameDocument) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kRegular, 1);
}

TEST(NavigationMetrics, MainFrameSchemeDifferentDocumentOTR) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, true,
      profile_metrics::BrowserProfileType::kIncognito);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPage, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeDifferentPageOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kIncognito, 1);
}

TEST(NavigationMetrics, MainFrameSchemeSameDocumentOTR) {
  base::HistogramTester test;

  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, true,
      profile_metrics::BrowserProfileType::kIncognito);

  test.ExpectTotalCount(kMainFrameScheme, 1);
  test.ExpectUniqueSample(kMainFrameScheme, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageNonUniqueHostname, 0);
  test.ExpectTotalCount(kMainFrameSchemeOTR, 1);
  test.ExpectUniqueSample(kMainFrameSchemeOTR, 1 /* http */, 1);
  test.ExpectTotalCount(kMainFrameSchemeDifferentPageOTR, 0);
  test.ExpectTotalCount(kMainFrameProfileType, 1);
  test.ExpectUniqueSample(kMainFrameProfileType,
                          profile_metrics::BrowserProfileType::kIncognito, 1);
}

TEST(NavigationMetrics, MainFrameDifferentDocumentHasRTLDomainFalse) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 1);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomainDifferentPage, 0 /* false */,
                          1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 0 /* false */, 1);
}

TEST(NavigationMetrics, MainFrameDifferentDocumentHasRTLDomainTrue) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kRtlUrl), false, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 1);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomainDifferentPage, 1 /* true */, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 1 /* true */, 1);
}

TEST(NavigationMetrics, MainFrameSameDocumentHasRTLDomainFalse) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kTestUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 0 /* false */, 1);
}

TEST(NavigationMetrics, MainFrameSameDocumentHasRTLDomainTrue) {
  base::HistogramTester test;
  RecordPrimaryMainFrameNavigation(
      GURL(kRtlUrl), true, false,
      profile_metrics::BrowserProfileType::kRegular);
  test.ExpectTotalCount(kMainFrameHasRTLDomainDifferentPage, 0);
  test.ExpectTotalCount(kMainFrameHasRTLDomain, 1);
  test.ExpectUniqueSample(kMainFrameHasRTLDomain, 1 /* true */, 1);
}

TEST(NavigationMetrics, RecordIDNA2008Metrics) {
  static constexpr char kHistogram[] =
      "Navigation.HostnameHasDeviationCharacters";
  base::HistogramTester histograms;

  // Shouldn't record metrics for non-unique hostnames.
  RecordIDNA2008Metrics(u"faß.local");
  histograms.ExpectTotalCount(kHistogram, 0);

  // Shouldn't record deviation characters in subdomains.
  RecordIDNA2008Metrics(u"faß.example.de");
  histograms.ExpectTotalCount(kHistogram, 1);
  histograms.ExpectBucketCount(kHistogram, false, 1);
  histograms.ExpectBucketCount(kHistogram, true, 0);

  // Shouldn't record deviation characters in subdomains of private registries.
  RecordIDNA2008Metrics(u"faß.blogspot.com");
  histograms.ExpectTotalCount(kHistogram, 2);
  histograms.ExpectBucketCount(kHistogram, false, 2);
  histograms.ExpectBucketCount(kHistogram, true, 0);

  // Positive tests.
  RecordIDNA2008Metrics(u"faß.de");
  histograms.ExpectTotalCount(kHistogram, 3);
  histograms.ExpectBucketCount(kHistogram, false, 2);
  histograms.ExpectBucketCount(kHistogram, true, 1);

  RecordIDNA2008Metrics(u"subdomain.faß.de");
  histograms.ExpectTotalCount(kHistogram, 4);
  histograms.ExpectBucketCount(kHistogram, false, 2);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  // Should work well with non-standard separators
  RecordIDNA2008Metrics(u"example。com");
  histograms.ExpectTotalCount(kHistogram, 5);
  histograms.ExpectBucketCount(kHistogram, false, 3);
  histograms.ExpectBucketCount(kHistogram, true, 2);

  RecordIDNA2008Metrics(u"subdomain。faß。de");
  histograms.ExpectTotalCount(kHistogram, 6);
  histograms.ExpectBucketCount(kHistogram, false, 3);
  histograms.ExpectBucketCount(kHistogram, true, 3);

  // Should drop edge cases like %-encoded separators (%2e is ".") and not
  // record metrics.
  RecordIDNA2008Metrics(u"subdomain%2efaß%2ede");
  histograms.ExpectTotalCount(kHistogram, 6);

  // Should drop edge cases where the label count check passes but the
  // canonicalized eTLD+1 and non-canonicalized eTLD+1 have diverged. (This case
  // should be rare and shouldn't cause crashes, but will result in potentially
  // junk metrics being collected.)
  RecordIDNA2008Metrics(u"abc.def.faß.example%2ecom");
  histograms.ExpectTotalCount(kHistogram, 6);
}

// Regression test for crbug.com/1362507. Tests that the IDNA2008 metrics code
// correctly handles hostnames with trailing dots.
TEST(NavigationMetrics, RecordIDNA2008MetricsTrailingDots) {
  static constexpr char kHistogram[] =
      "Navigation.HostnameHasDeviationCharacters";
  base::HistogramTester histograms;

  // This would previously trigger the DCHECK in GetEtldPlusOne16(), or cause
  // a crash in the std::vector::erase() call later in that function, because
  // the canonicalized hostname has two dots and so the logic would calculate
  // this as having three labels, but the noncanonicalized labels vector would
  // only have two elements due to dropping the final empty string after the
  // trailing dot.
  RecordIDNA2008Metrics(u"googlé.com.");
  histograms.ExpectTotalCount(kHistogram, 1);

  // GetDomainAndRegistry() should return the empty string for this hostname,
  // so no metrics will be recorded.
  RecordIDNA2008Metrics(u"googlé.com..");
  histograms.ExpectTotalCount(kHistogram, 1);

  // Should be treated the same as the "googlé.com" case.
  RecordIDNA2008Metrics(u".googlé.com");
  histograms.ExpectTotalCount(kHistogram, 2);

  // Should be treated the same as the "googlé.com." case.
  RecordIDNA2008Metrics(u".googlé.com.");
  histograms.ExpectTotalCount(kHistogram, 3);
}

}  // namespace navigation_metrics
