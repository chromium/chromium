// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/baseline_language_model.h"

#include <cmath>

#include "components/language/core/browser/url_language_histogram.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

using testing::ElementsAre;
using Ld = BaselineLanguageModel::LanguageDetails;

constexpr static float kFloatEps = 0.00001f;
constexpr static char kAcceptPref[] = "intl.accept_languages";

class BaselineLanguageModelTest : public testing::Test {
 protected:
  void SetUp() override {
    // The URL language histogram and accept language list are inputs to the
    // baseline model.
    UrlLanguageHistogram::RegisterProfilePrefs(prefs_.registry());
    prefs_.registry()->RegisterStringPref(kAcceptPref, std::string());
  }

  TestingPrefServiceSimple prefs_;
};

// Compares LanguageDetails.
MATCHER_P(EqualsLd, lang_details, "") {
  return arg.lang_code == lang_details.lang_code &&
         std::abs(arg.score - lang_details.score) < kFloatEps;
}

// Check the minimum model: just a UI language.
TEST_F(BaselineLanguageModelTest, UiOnly) {
  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(model.GetLanguages(), ElementsAre(EqualsLd(Ld("en", 1.0f))));
}

// Check with UI language and language browsing history.
TEST_F(BaselineLanguageModelTest, UiAndHist) {
  // Simulate many website visits, as there is a minimum frequency threshold for
  // histogram languages.
  UrlLanguageHistogram hist(&prefs_);
  for (int i = 0; i < 100; ++i) {
    hist.OnPageVisited("it");
    hist.OnPageVisited("it");
    hist.OnPageVisited("de");
  }

  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(model.GetLanguages(), ElementsAre(EqualsLd(Ld("en", 1.0f)),
                                                EqualsLd(Ld("it", 1.0f / 2)),
                                                EqualsLd(Ld("de", 1.0f / 3))));
}

// Check with UI language and accept languages.
TEST_F(BaselineLanguageModelTest, UiAndAccept) {
  prefs_.SetString(kAcceptPref, "ja,fr");

  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(model.GetLanguages(), ElementsAre(EqualsLd(Ld("en", 1.0f)),
                                                EqualsLd(Ld("ja", 1.0f / 2)),
                                                EqualsLd(Ld("fr", 1.0f / 3))));
}

// Check with all three sources.
TEST_F(BaselineLanguageModelTest, UiAndHistAndAccept) {
  // Simulate many website visits, as there is a minimum frequency threshold for
  // histogram languages.
  UrlLanguageHistogram hist(&prefs_);
  for (int i = 0; i < 100; ++i) {
    hist.OnPageVisited("it");
    hist.OnPageVisited("it");
    hist.OnPageVisited("de");
  }

  prefs_.SetString(kAcceptPref, "ja,fr");

  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(
      model.GetLanguages(),
      ElementsAre(EqualsLd(Ld("en", 1.0f)), EqualsLd(Ld("it", 1.0f / 2)),
                  EqualsLd(Ld("de", 1.0f / 3)), EqualsLd(Ld("ja", 1.0f / 4)),
                  EqualsLd(Ld("fr", 1.0f / 5))));
}

// Check that repeats among sources are ignored.
TEST_F(BaselineLanguageModelTest, Repeats) {
  // Simulate many website visits, as there is a minimum frequency threshold for
  // histogram languages.
  UrlLanguageHistogram hist(&prefs_);
  for (int i = 0; i < 100; ++i) {
    hist.OnPageVisited("en");
    hist.OnPageVisited("en");
    hist.OnPageVisited("it");
  }

  prefs_.SetString(kAcceptPref, "en,ja,it");

  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(model.GetLanguages(), ElementsAre(EqualsLd(Ld("en", 1.0f)),
                                                EqualsLd(Ld("it", 1.0f / 2)),
                                                EqualsLd(Ld("ja", 1.0f / 3))));
}

// Check that regions are not backed off.
TEST_F(BaselineLanguageModelTest, Regions) {
  // Simulate many website visits, as there is a minimum frequency threshold for
  // histogram languages.
  UrlLanguageHistogram hist(&prefs_);
  for (int i = 0; i < 100; ++i) {
    hist.OnPageVisited("en-AU");
    hist.OnPageVisited("en-AU");
    hist.OnPageVisited("en-CA");
  }

  prefs_.SetString(kAcceptPref, "en-GB");

  BaselineLanguageModel model(&prefs_, "en", kAcceptPref);
  EXPECT_THAT(
      model.GetLanguages(),
      ElementsAre(EqualsLd(Ld("en", 1.0f)), EqualsLd(Ld("en-AU", 1.0f / 2)),
                  EqualsLd(Ld("en-CA", 1.0f / 3)),
                  EqualsLd(Ld("en-GB", 1.0f / 4))));
}

}  // namespace language
