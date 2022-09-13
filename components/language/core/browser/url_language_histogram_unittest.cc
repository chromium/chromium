// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/url_language_histogram.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::FloatEq;
using testing::Gt;
using testing::SizeIs;

namespace {

const char kLang1[] = "en";
const char kLang2[] = "de";
const char kLang3[] = "es";

}  // namespace

namespace language {

bool operator==(const UrlLanguageHistogram::LanguageInfo& lhs,
                const UrlLanguageHistogram::LanguageInfo& rhs) {
  return lhs.language_code == rhs.language_code;
}

TEST(UrlLanguageHistogramTest, ListSorted) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  for (int i = 0; i < 50; i++) {
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang2);
  }

  // Note: LanguageInfo's operator== only checks the language code, not the
  // frequency.
  EXPECT_THAT(hist.GetTopLanguages(),
              ElementsAre(UrlLanguageHistogram::LanguageInfo(kLang1, 0.0f),
                          UrlLanguageHistogram::LanguageInfo(kLang2, 0.0f)));
}

TEST(UrlLanguageHistogramTest, ListSortedReversed) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  for (int i = 0; i < 50; i++) {
    hist.OnPageVisited(kLang2);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
  }

  // Note: LanguageInfo's operator== only checks the language code, not the
  // frequency.
  EXPECT_THAT(hist.GetTopLanguages(),
              ElementsAre(UrlLanguageHistogram::LanguageInfo(kLang1, 0.0f),
                          UrlLanguageHistogram::LanguageInfo(kLang2, 0.0f)));
}

TEST(UrlLanguageHistogramTest, RightFrequencies) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  for (int i = 0; i < 50; i++) {
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang1);
    hist.OnPageVisited(kLang2);
  }

  // Corresponding frequencies are given by the hist.
  EXPECT_THAT(hist.GetLanguageFrequency(kLang1), FloatEq(0.75f));
  EXPECT_THAT(hist.GetLanguageFrequency(kLang2), FloatEq(0.25f));
  // An unknown language gets frequency 0.
  EXPECT_THAT(hist.GetLanguageFrequency(kLang3), 0);
}

TEST(UrlLanguageHistogramTest, RareLanguageDiscarded) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  hist.OnPageVisited(kLang2);

  for (int i = 0; i < 900; i++)
    hist.OnPageVisited(kLang1);

  // Lang 2 is in the hist.
  EXPECT_THAT(hist.GetLanguageFrequency(kLang2), Gt(0.0f));

  // Another 100 visits cause the cleanup (total > 1000).
  for (int i = 0; i < 100; i++)
    hist.OnPageVisited(kLang1);
  // Lang 2 is removed from the hist.
  EXPECT_THAT(hist.GetTopLanguages(),
              ElementsAre(UrlLanguageHistogram::LanguageInfo{kLang1, 1}));
}

TEST(UrlLanguageHistogramTest, ShouldClearHistoryIfAllTimes) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  for (int i = 0; i < 100; i++) {
    hist.OnPageVisited(kLang1);
  }

  EXPECT_THAT(hist.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(hist.GetLanguageFrequency(kLang1), FloatEq(1.0));

  hist.ClearHistory(base::Time(), base::Time::Max());

  EXPECT_THAT(hist.GetTopLanguages(), SizeIs(0));
  EXPECT_THAT(hist.GetLanguageFrequency(kLang1), FloatEq(0.0));
}

TEST(UrlLanguageHistogramTest, ShouldNotClearHistoryIfNotAllTimes) {
  TestingPrefServiceSimple prefs;
  UrlLanguageHistogram::RegisterProfilePrefs(prefs.registry());
  UrlLanguageHistogram hist(&prefs);

  for (int i = 0; i < 100; i++) {
    hist.OnPageVisited(kLang1);
  }

  EXPECT_THAT(hist.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(hist.GetLanguageFrequency(kLang1), FloatEq(1.0));

  // Clearing only the last hour of the history has no effect.
  hist.ClearHistory(base::Time::Now() - base::Hours(2), base::Time::Max());

  EXPECT_THAT(hist.GetTopLanguages(), SizeIs(1));
  EXPECT_THAT(hist.GetLanguageFrequency(kLang1), FloatEq(1.0));
}

}  // namespace language
