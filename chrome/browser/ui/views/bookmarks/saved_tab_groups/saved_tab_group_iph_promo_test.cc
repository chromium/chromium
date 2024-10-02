// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"

class SavedTabGroupV2PromoTest : public InteractiveFeaturePromoTest,
                                 public testing::WithParamInterface<bool> {
 public:
  SavedTabGroupV2PromoTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHTabGroupsSaveV2CloseGroupFeature})) {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {{tab_groups::kTabGroupSyncServiceDesktopMigration,
            tab_groups::kTabGroupsSaveV2, tab_groups::kTabGroupsSaveUIUpdate}},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {{tab_groups::kTabGroupsSaveV2, tab_groups::kTabGroupsSaveUIUpdate}},
          {});
    }
  }

  ~SavedTabGroupV2PromoTest() override = default;

  auto TriggerPromo() {
    auto steps = Steps(
        Do([this]() {
          tab_groups::TabGroupSyncService* service =
              tab_groups::SavedTabGroupUtils::GetServiceForProfile(
                  browser()->profile());
          ASSERT_TRUE(service);
          service->SetIsInitializedForTesting(true);

          chrome::AddTabAt(browser(), GURL(), 0, true);
          chrome::AddTabAt(browser(), GURL(), 1, true);
          tab_groups::TabGroupId group_id =
              browser()->tab_strip_model()->AddToNewGroup({0});

          tab_groups::SavedTabGroupUtils::RemoveGroupFromTabstrip(browser(),
                                                                  group_id);
        }),
        WaitForPromo(feature_engagement::kIPHTabGroupsSaveV2CloseGroupFeature));
    AddDescription(steps, "SaveAndCloseGroup( %s )");
    return steps;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SavedTabGroupV2PromoTest,
                       TestShowingIPHOnSavedTabGroupBar) {
  // Show the SavedTabGroupBar and the BookmarkBar.
  PrefService* prefs = browser()->profile()->GetPrefs();
  const bool original_stgb_pref =
      prefs->GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar);
  prefs->SetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar, true);

  RunTestSequence(
      TriggerPromo(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));

  // reset the pref to the expected_value.
  prefs->SetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar,
                    original_stgb_pref);
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupV2PromoTest,
                       TestShowingIPHWithoutSavedTabGroupBar) {
  // Show the SavedTabGroupBar and the BookmarkBar.
  PrefService* prefs = browser()->profile()->GetPrefs();
  const bool original_stgb_pref =
      prefs->GetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar);
  prefs->SetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar, false);

  RunTestSequence(
      TriggerPromo(), PressButton(kToolbarAppMenuButtonElementId),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting));

  // reset the pref to the expected_value.
  prefs->SetBoolean(bookmarks::prefs::kShowTabGroupsInBookmarkBar,
                    original_stgb_pref);
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupV2Promo,
                         SavedTabGroupV2PromoTest,
                         testing::Bool());
