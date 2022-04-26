// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_provider.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/app_list_notifier_impl_old.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace test {

namespace {

constexpr char kDiscoverTabResultId[] = "help-app://discover";
constexpr char kReleaseNotesResultId[] = "help-app://updates";

void ExpectDiscoverTabChip(ChromeSearchResult* result) {
  EXPECT_EQ(kDiscoverTabResultId, result->id());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_SUGGESTION_CHIP),
      result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kHelpApp, result->result_type());
  EXPECT_EQ(ash::SearchResultDisplayType::kChip, result->display_type());
}

void ExpectReleaseNotesChip(ChromeSearchResult* result,
                            ash::SearchResultDisplayType display_type) {
  EXPECT_EQ(kReleaseNotesResultId, result->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP),
            result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kHelpApp, result->result_type());
  EXPECT_EQ(display_type, result->display_type());
}
}  // namespace

class HelpAppProviderTest : public AppListTestBase {
 public:
  HelpAppProviderTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kHelpAppDiscoverTab,
                              chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{});
  }
  ~HelpAppProviderTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();

    app_list_notifier_ =
        std::make_unique<AppListNotifierImplOld>(&app_list_controller_);

    provider_ =
        std::make_unique<HelpAppProvider>(profile(), app_list_notifier_.get());
    provider_->set_controller(&search_controller_);
  }

  const app_list::Results& GetLatestResults() { return provider()->results(); }

  ::test::TestAppListController* app_list_controller() {
    return &app_list_controller_;
  }

  ash::AppListNotifier* app_list_notifier() { return app_list_notifier_.get(); }

  HelpAppProvider* provider() { return provider_.get(); }

 private:
  ::test::TestAppListController app_list_controller_;
  std::unique_ptr<ash::AppListNotifier> app_list_notifier_;
  TestSearchController search_controller_;
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

  EXPECT_TRUE(GetLatestResults().empty());
}

TEST_F(HelpAppProviderTest,
       ReturnsDiscoverTabChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectDiscoverTabChip(result);
}

TEST_F(HelpAppProviderTest,
       ReturnsReleaseNotesChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, ash::SearchResultDisplayType::kChip);
}

TEST_F(HelpAppProviderTest, PrioritizesDiscoverTabChipForEmptyQuery) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectDiscoverTabChip(result);
}

TEST_F(HelpAppProviderTest,
       DecrementsTimesLeftToShowDiscoverTabChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectDiscoverTabChip(result);

  app_list_controller()->ShowAppList();
  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));

  app_list_notifier()->NotifyResultsUpdated(
      ash::AppListNotifier::Location::kChip,
      {ash::AppListNotifier::Result(kDiscoverTabResultId,
                                    ash::HELP_APP_UPDATES)});
  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
  ASSERT_TRUE(app_list_notifier()->FireImpressionTimerForTesting(
      ash::AppListNotifier::Location::kChip));

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest,
       DecrementsTimesLeftToShowReleaseNotesChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, ash::SearchResultDisplayType::kChip);

  app_list_controller()->ShowAppList();
  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  app_list_notifier()->NotifyResultsUpdated(
      ash::SearchResultDisplayType::kChip,
      {ash::AppListNotifier::Result(kReleaseNotesResultId,
                                    ash::HELP_APP_UPDATES)});

  ASSERT_TRUE(app_list_notifier()->FireImpressionTimerForTesting(
      ash::SearchResultDisplayType::kChip));

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest, ClickingDiscoverTabChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

TEST_F(HelpAppProviderTest, ClickingReleaseNotesChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  provider()->StartZeroState();

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

class HelpAppProviderWithDiscoverTabDisabledTest : public HelpAppProviderTest {
 public:
  HelpAppProviderWithDiscoverTabDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{chromeos::features::kHelpAppDiscoverTab});
  }
  ~HelpAppProviderWithDiscoverTabDisabledTest() override = default;

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

  EXPECT_TRUE(GetLatestResults().empty());
}

}  // namespace test
}  // namespace app_list
