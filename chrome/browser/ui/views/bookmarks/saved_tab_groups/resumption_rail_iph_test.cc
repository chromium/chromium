// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
constexpr int kNumTabGroupsToForceOverflow = 30;
}  // namespace

class ResumptionRailPromoTest : public InteractiveFeaturePromoTest {
 public:
  ResumptionRailPromoTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHResumptionRailFeature,
             feature_engagement::kIPHReadingListDiscoveryFeature})) {
    feature_list_.InitWithFeatures(
        {feature_engagement::kIPHResumptionRailFeature,
         feature_engagement::kIPHReadingListDiscoveryFeature,
         tab_groups::kProjectsPanel, tabs::kHorizontalTabStripComboButton},
        {});
  }

  ~ResumptionRailPromoTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kProjectsPanelPinnedToTabstrip, true);
    ProjectsPanelView::disable_animations_for_testing();

    // Ensure the bookmarks bar is shown so the overflow button can be
    // visible.
    BookmarkBarView::DisableAnimationsForTesting(true);
    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);
    if (!browser()->window()->IsBookmarkBarVisible()) {
      chrome::ExecuteCommand(browser(), IDC_SHOW_BOOKMARK_BAR);
    }
    ASSERT_TRUE(tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
        browser()->profile()));
  }

  auto AddTabGroupsToForceOverflow() {
    return Do([this]() {
      auto* service = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());
      for (int i = 0; i < kNumTabGroupsToForceOverflow; ++i) {
        base::Uuid id = base::Uuid::ParseLowercase(
            base::StringPrintf("00000000-0000-0000-0000-0000000000%02d", i));
        tab_groups::SavedTabGroup group(u"Group " + base::NumberToString16(i),
                                        tab_groups::TabGroupColorId::kBlue, {},
                                        std::nullopt, id);
        tab_groups::SavedTabGroupTab tab(GURL("about:blank"), u"Tab", id,
                                         std::nullopt);
        group.AddTabLocally(std::move(tab));
        service->AddGroup(std::move(group));
      }
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest, TriggerPromo) {
  RunTestSequence(WaitForShow(kBookmarkBarElementId),
                  Do([this]() { RunScheduledLayouts(); }),
                  WaitForShow(kVerticalTabStripProjectsButtonElementId),
                  // Add enough groups to force the overflow button to appear.
                  AddTabGroupsToForceOverflow(),
                  WaitForShow(kSavedTabGroupBarElementId),
                  WaitForShow(kSavedTabGroupOverflowButtonElementId),
                  // Click the legacy Everything button.
                  PressButton(kSavedTabGroupOverflowButtonElementId),
                  // The IPH SHOULD trigger, and the menu should not open.
                  WaitForPromo(feature_engagement::kIPHResumptionRailFeature),
                  // Click the new Projects button to dismiss the promo.
                  PressButton(kVerticalTabStripProjectsButtonElementId),
                  WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest,
                       OpenProjectsPanelThenTriggerPromo) {
  RunTestSequence(WaitForShow(kBookmarkBarElementId),
                  Do([this]() { RunScheduledLayouts(); }),
                  WaitForShow(kVerticalTabStripProjectsButtonElementId),
                  // Add enough groups to force the overflow button to appear.
                  AddTabGroupsToForceOverflow(),
                  WaitForShow(kSavedTabGroupBarElementId),
                  WaitForShow(kSavedTabGroupOverflowButtonElementId),
                  // Click the new Projects button.
                  PressButton(kVerticalTabStripProjectsButtonElementId),
                  // The projects panel should show.
                  WaitForShow(kProjectsPanelViewElementId),
                  // Click the new Projects button again to close it.
                  PressButton(kVerticalTabStripProjectsButtonElementId),
                  WaitForHide(kProjectsPanelViewElementId),
                  // Click the legacy Everything button immediately after.
                  PressButton(kSavedTabGroupOverflowButtonElementId),
                  // The IPH SHOULD trigger, and the menu should not open.
                  WaitForPromo(feature_engagement::kIPHResumptionRailFeature),
                  // Click the new Projects button to dismiss the promo.
                  PressButton(kVerticalTabStripProjectsButtonElementId),
                  // Should hide the Everything menu button.
                  WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest,
                       ClosePromoWhenProjectsPanelOpens) {
  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kVerticalTabStripProjectsButtonElementId),
      // Add enough groups to force the overflow button to appear.
      AddTabGroupsToForceOverflow(), WaitForShow(kSavedTabGroupBarElementId),
      WaitForShow(kSavedTabGroupOverflowButtonElementId),
      // Click the legacy Everything button.
      PressButton(kSavedTabGroupOverflowButtonElementId),
      // The IPH SHOULD trigger, and the menu should not open.
      WaitForPromo(feature_engagement::kIPHResumptionRailFeature),
      // Click the new Projects button.
      PressButton(kVerticalTabStripProjectsButtonElementId),
      // The projects panel should show.
      WaitForShow(kProjectsPanelViewElementId),
      // The promo should be dismissed.
      CheckPromoActive(feature_engagement::kIPHResumptionRailFeature, false),
      // Should hide the Everything menu button.
      WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest, QueuePromoIfAnotherActive) {
  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kVerticalTabStripProjectsButtonElementId),
      // Add enough groups to force the overflow button to appear.
      AddTabGroupsToForceOverflow(), WaitForShow(kSavedTabGroupBarElementId),
      WaitForShow(kSavedTabGroupOverflowButtonElementId),
      // Show another promo first to block the resumption rail promo.
      Do([this]() {
        if (auto* interface = BrowserUserEducationInterface::From(browser())) {
          interface->MaybeShowFeaturePromo(
              feature_engagement::kIPHReadingListDiscoveryFeature);
        }
      }),
      WaitForPromo(feature_engagement::kIPHReadingListDiscoveryFeature),
      // Click the legacy Everything button. The resumption rail promo should be
      // queued.
      PressButton(kSavedTabGroupOverflowButtonElementId),
      // The other promo should still be active.
      CheckPromoActive(feature_engagement::kIPHReadingListDiscoveryFeature,
                       true),
      // The resumption rail promo should not be active yet.
      CheckPromoActive(feature_engagement::kIPHResumptionRailFeature, false),
      // Close the first promo.
      PressClosePromoButton(),
      // The resumption rail promo should now show.
      WaitForPromo(feature_engagement::kIPHResumptionRailFeature),
      // Click the new Projects button to dismiss the promo.
      PressButton(kVerticalTabStripProjectsButtonElementId),
      // Should hide the Everything menu button.
      WaitForHide(kSavedTabGroupOverflowButtonElementId));
}
