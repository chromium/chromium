// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_provider.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace test {

namespace {
void ExpectDiscoverTabChip(ChromeSearchResult* result) {
  EXPECT_EQ("help-app://discover", result->id());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_SUGGESTION_CHIP),
      result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kHelpApp, result->result_type());
  EXPECT_EQ(ash::SearchResultDisplayType::kChip, result->display_type());
}

void ExpectReleaseNotesChip(ChromeSearchResult* result) {
  EXPECT_EQ("help-app://updates", result->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP),
            result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kHelpApp, result->result_type());
  EXPECT_EQ(ash::SearchResultDisplayType::kChip, result->display_type());
}
}  // namespace

class HelpAppProviderTest : public AppListTestBase {
 public:
  HelpAppProviderTest() {}
  ~HelpAppProviderTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();

    provider_ = std::make_unique<HelpAppProvider>(profile());
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kHelpAppDiscoverTab,
                              chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{});
  }

  HelpAppProvider* provider() { return provider_.get(); }

 private:
  std::unique_ptr<HelpAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test for empty query.
TEST_F(HelpAppProviderTest, HasNoResultsForEmptyQueryIfTimesLeftToShowIsZero) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  provider()->StartZeroState();

  EXPECT_TRUE(provider()->results().empty());
}

TEST_F(HelpAppProviderTest,
       ReturnsDiscoverTabChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  provider()->StartZeroState();

  EXPECT_EQ(1u, provider()->results().size());
  ChromeSearchResult* result = provider()->results().at(0).get();
  ExpectDiscoverTabChip(result);
}

TEST_F(HelpAppProviderTest,
       ReturnsReleaseNotesChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  provider()->StartZeroState();

  EXPECT_EQ(1u, provider()->results().size());
  ChromeSearchResult* result = provider()->results().at(0).get();
  ExpectReleaseNotesChip(result);
}

TEST_F(HelpAppProviderTest, PrioritizesDiscoverTabChipForEmptyQuery) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  provider()->StartZeroState();

  EXPECT_EQ(1u, provider()->results().size());
  ChromeSearchResult* result = provider()->results().at(0).get();
  ExpectDiscoverTabChip(result);
}

TEST_F(HelpAppProviderTest,
       DecrementsTimesLeftToShowDiscoverTabChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();
  provider()->AppListShown();

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest,
       DecrementsTimesLeftToShowReleaseNotesChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();
  provider()->AppListShown();

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest, ClickingDiscoverTabChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ChromeSearchResult* result = provider()->results().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest, ClickingReleaseNotesChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ChromeSearchResult* result = provider()->results().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

class HelpAppProviderWithDiscoverTabDisabledTest : public HelpAppProviderTest {
 public:
  HelpAppProviderWithDiscoverTabDisabledTest() {}
  ~HelpAppProviderWithDiscoverTabDisabledTest() override = default;

  void SetUp() override {
    HelpAppProviderTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{chromeos::features::kHelpAppDiscoverTab});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HelpAppProviderWithDiscoverTabDisabledTest,
       DoesNotReturnDiscoverTabChipForEmptyQuery) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  provider()->StartZeroState();

  EXPECT_TRUE(provider()->results().empty());
}

}  // namespace test
}  // namespace app_list
