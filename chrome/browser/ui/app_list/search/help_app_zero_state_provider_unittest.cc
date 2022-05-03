// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/help_app_zero_state_provider.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/app_list/app_list_notifier_impl.h"
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
  EXPECT_EQ(ash::AppListSearchResultType::kZeroStateHelpApp,
            result->result_type());
  EXPECT_EQ(ash::SearchResultDisplayType::kChip, result->display_type());
}

void ExpectReleaseNotesChip(ChromeSearchResult* result,
                            int title_id,
                            ash::SearchResultDisplayType display_type) {
  EXPECT_EQ(kReleaseNotesResultId, result->id());
  EXPECT_EQ(l10n_util::GetStringUTF16(title_id), result->title());
  EXPECT_EQ(ash::AppListSearchResultType::kZeroStateHelpApp,
            result->result_type());
  EXPECT_EQ(display_type, result->display_type());
}

}  // namespace

// Parameterized by whether ProductivityLauncher feature is enabled.
class HelpAppZeroStateProviderTest
    : public AppListTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  HelpAppZeroStateProviderTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kHelpAppDiscoverTab,
                                chromeos::features::kReleaseNotesSuggestionChip,
                                ash::features::kProductivityLauncher},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{chromeos::features::kHelpAppDiscoverTab,
                                chromeos::features::
                                    kReleaseNotesSuggestionChip},
          /*disabled_features=*/{ash::features::kProductivityLauncher});
    }
  }
  ~HelpAppZeroStateProviderTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();

    if (GetParam()) {
      app_list_notifier_ =
          std::make_unique<AppListNotifierImpl>(&app_list_controller_);
    } else {
      app_list_notifier_ =
          std::make_unique<AppListNotifierImplOld>(&app_list_controller_);
    }

    auto provider = std::make_unique<HelpAppZeroStateProvider>(
        profile(), app_list_notifier_.get());
    provider_ = provider.get();
    search_controller_.AddProvider(0, std::move(provider));
  }

  ash::SearchResultDisplayType GetExpectedReleaseNotesDisplayType() {
    return GetParam() ? ash::SearchResultDisplayType::kContinue
                      : ash::SearchResultDisplayType::kChip;
  }

  int GetExpectedReleaseNotesTitleStringId() {
    return GetParam() ? IDS_HELP_APP_WHATS_NEW_CONTINUE_TASK_TITLE
                      : IDS_HELP_APP_WHATS_NEW_SUGGESTION_CHIP;
  }

  void StartZeroStateSearch() {
    search_controller_.StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const app_list::Results& GetLatestResults() {
    // When productivity launcher (and thus categorical search) is enabled,
    // results are managed by the search controller instead of individual search
    // providers.
    if (GetParam())
      return search_controller_.last_results();
    return provider_->results();
  }

  ::test::TestAppListController* app_list_controller() {
    return &app_list_controller_;
  }

  ash::AppListNotifier* app_list_notifier() { return app_list_notifier_.get(); }

 private:
  ::test::TestAppListController app_list_controller_;
  std::unique_ptr<ash::AppListNotifier> app_list_notifier_;
  TestSearchController search_controller_;
  HelpAppZeroStateProvider* provider_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         HelpAppZeroStateProviderTest,
                         testing::Bool());

// Test for empty query.
TEST_P(HelpAppZeroStateProviderTest,
       HasNoResultsForEmptyQueryIfTimesLeftToShowIsZero) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  StartZeroStateSearch();

  EXPECT_TRUE(GetLatestResults().empty());
}

TEST_P(HelpAppZeroStateProviderTest,
       ReturnsDiscoverTabChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  StartZeroStateSearch();

  ASSERT_EQ(GetParam() ? 0u : 1u, GetLatestResults().size());
  if (GetParam())
    return;

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectDiscoverTabChip(result);
}

TEST_P(HelpAppZeroStateProviderTest,
       ReturnsReleaseNotesChipForEmptyQueryIfTimesLeftIsPositive) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 0);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  StartZeroStateSearch();

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, GetExpectedReleaseNotesTitleStringId(),
                         GetExpectedReleaseNotesDisplayType());
}

TEST_P(HelpAppZeroStateProviderTest, PrioritizesDiscoverTabChipForEmptyQuery) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  StartZeroStateSearch();

  ASSERT_EQ(1u, GetLatestResults().size());

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  if (GetParam()) {
    ExpectReleaseNotesChip(result, GetExpectedReleaseNotesTitleStringId(),
                           GetExpectedReleaseNotesDisplayType());
  } else {
    ExpectDiscoverTabChip(result);
  }
}

TEST_P(HelpAppZeroStateProviderTest,
       DecrementsTimesLeftToShowDiscoverTabChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  if (GetParam()) {
    EXPECT_EQ(0u, GetLatestResults().size());
    return;
  }

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

TEST_P(HelpAppZeroStateProviderTest,
       DecrementsTimesLeftToShowReleaseNotesChipUponShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  ASSERT_EQ(1u, GetLatestResults().size());

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  ExpectReleaseNotesChip(result, GetExpectedReleaseNotesTitleStringId(),
                         GetExpectedReleaseNotesDisplayType());

  app_list_controller()->ShowAppList();
  EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));

  app_list_notifier()->NotifyResultsUpdated(
      GetExpectedReleaseNotesDisplayType(),
      {ash::AppListNotifier::Result(kReleaseNotesResultId,
                                    ash::HELP_APP_UPDATES)});
  if (GetParam()) {
    EXPECT_FALSE(app_list_notifier()->FireImpressionTimerForTesting(
        ash::AppListNotifier::Location::kContinue));

    app_list_notifier()->NotifyContinueSectionVisibilityChanged(
        ash::AppListNotifier::Location::kContinue, true);

    EXPECT_EQ(3, profile()->GetPrefs()->GetInteger(
                     prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
  }

  ASSERT_TRUE(app_list_notifier()->FireImpressionTimerForTesting(
      GetExpectedReleaseNotesDisplayType()));

  EXPECT_EQ(2, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

TEST_P(HelpAppZeroStateProviderTest,
       ClickingDiscoverTabChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  if (GetParam()) {
    EXPECT_EQ(0u, GetLatestResults().size());
    return;
  }

  ASSERT_EQ(1u, GetLatestResults().size());
  ChromeSearchResult* result = GetLatestResults().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kDiscoverTabSuggestionChipTimesLeftToShow));
}

TEST_P(HelpAppZeroStateProviderTest,
       ClickingReleaseNotesChipStopsItFromShowing) {
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 3);

  StartZeroStateSearch();

  ChromeSearchResult* result = GetLatestResults().at(0).get();
  result->Open(/*event_flags=*/0);

  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
}

class HelpAppZeroStateProviderWithDiscoverTabDisabledTest
    : public HelpAppZeroStateProviderTest {
 public:
  HelpAppZeroStateProviderWithDiscoverTabDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{chromeos::features::kHelpAppDiscoverTab});
  }
  ~HelpAppZeroStateProviderWithDiscoverTabDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(ProductivityLauncher,
                         HelpAppZeroStateProviderWithDiscoverTabDisabledTest,
                         testing::Bool());

TEST_P(HelpAppZeroStateProviderWithDiscoverTabDisabledTest,
       DoesNotReturnDiscoverTabChipForEmptyQuery) {
  profile()->GetPrefs()->SetInteger(
      prefs::kDiscoverTabSuggestionChipTimesLeftToShow, 1);
  profile()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  StartZeroStateSearch();

  EXPECT_TRUE(GetLatestResults().empty());
}

}  // namespace test
}  // namespace app_list
