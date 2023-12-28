// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_side_panel_coordinator.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace {

constexpr char kNewLensQueryAction[] = "LensUnifiedSidePanel.LensQuery_New";
constexpr char kLensQueryAction[] = "LensUnifiedSidePanel.LensQuery";
constexpr char kLensEntryHiddenAction[] =
    "LensUnifiedSidePanel.LensEntryHidden";
constexpr char kLensEntryShownAction[] = "LensUnifiedSidePanel.LensEntryShown";
constexpr char kLensQueryFollowupAction[] =
    "LensUnifiedSidePanel.LensQuery_Followup";
constexpr char kLensQuerySidePanelClosedAction[] =
    "LensUnifiedSidePanel.LensQuery_SidePanelClosed";
constexpr char kLensQuerySidePanelOpenNonLensAction[] =
    "LensUnifiedSidePanel.LensQuery_SidePanelOpenNonLens";
constexpr char kLensQuerySidePanelOpenLensAction[] =
    "LensUnifiedSidePanel.LensQuery_SidePanelOpenLens";

constexpr char kLensHomepageURL[] = "http://foo.com";
constexpr char kLensAlternateHomepageURL[] = "http://foo.com/alternate1";

class LensSidePanelCoordinatorTest
    : public TestWithBrowserView,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (GetParam()) {
      features.InitWithFeaturesAndParameters(
          {{lens::features::kLensStandalone,
            {{lens::features::kHomepageURLForLens.name, kLensHomepageURL}}},
           {features::kSidePanelPinning, {}},
           {features::kChromeRefresh2023, {}}},
          {});
    } else {
      features.InitWithFeaturesAndParameters(
          {{lens::features::kLensStandalone,
            {{lens::features::kHomepageURLForLens.name, kLensHomepageURL}}}},
          {});
    }
    TestWithBrowserView::SetUp();

    GetSidePanelCoordinator()->SetNoDelaysForTesting(true);
    auto* browser = browser_view()->browser();
    auto* global_registry =
        SidePanelCoordinator::GetGlobalSidePanelRegistry(browser);
    SidePanelUtil::PopulateGlobalEntries(browser, global_registry);

    // Reading list, bookmarks, reading mode.
    EXPECT_EQ(global_registry->entries().size(), 3u);

    // Create the lens coordinator in Browser.
    lens_side_panel_coordinator_ =
        LensSidePanelCoordinator::GetOrCreateForBrowser(browser);
    // Create an active web contents.
    AddTab(browser, GURL("about:blank"));
  }

  void TearDown() override {
    lens_side_panel_coordinator_ = nullptr;
    TestWithBrowserView::TearDown();
  }

  SidePanelCoordinator* GetSidePanelCoordinator() {
    return SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  }

  SidePanel* GetUnifiedSidePanel() {
    return browser_view()->unified_side_panel();
  }

  actions::ActionItem* GetActionItem() {
    BrowserActions* browser_actions = BrowserActions::FromBrowser(browser());
    return actions::ActionManager::Get().FindAction(
        kActionSidePanelShowLens, browser_actions->root_action_item());
  }

 protected:
  // If ScopedFeatureList goes out of scope, the features are reset.
  base::test::ScopedFeatureList features;
  raw_ptr<LensSidePanelCoordinator> lens_side_panel_coordinator_;
};

TEST_P(LensSidePanelCoordinatorTest,
       OpenWithUrlShowsUnifiedSidePanelWithLensSelected) {
  base::UserActionTester user_action_tester;
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNewLensQueryAction));

  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kLens);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensQueryAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNewLensQueryAction));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kLensQuerySidePanelClosedAction));
}

TEST_P(LensSidePanelCoordinatorTest, ActionItemTest) {
  if (!base::FeatureList::IsEnabled(features::kSidePanelPinning)) {
    return;
  }

  actions::ActionItem* lens_action_item = GetActionItem();
  EXPECT_TRUE(lens_action_item);
  EXPECT_EQ(lens_action_item->GetText(),
            l10n_util::GetStringUTF16(IDS_GOOGLE_LENS_TITLE));
  EXPECT_EQ(lens_action_item->GetImage(),
            ui::ImageModel::FromVectorIcon(vector_icons::kGoogleLensLogoIcon));

  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  lens_action_item->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(
              kSidePanelOpenTriggerKey,
              static_cast<std::underlying_type_t<SidePanelOpenTrigger>>(
                  SidePanelOpenTrigger::kPinnedEntryToolbarButton))
          .Build());
  EXPECT_FALSE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(lens_action_item->GetInvokeCount(), 1);
}

TEST_P(LensSidePanelCoordinatorTest, OpenWithUrlWhenSidePanelOpenShowsLens) {
  base::UserActionTester user_action_tester;
  GetSidePanelCoordinator()->Show(SidePanelEntry::Id::kBookmarks);
  EXPECT_EQ(0, user_action_tester.GetActionCount(kNewLensQueryAction));

  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kLens);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNewLensQueryAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   kLensQuerySidePanelOpenNonLensAction));
}

TEST_P(LensSidePanelCoordinatorTest, DeregisterLensWithSidePanelOpen) {
  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kLens);
  auto* registry = SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kLens));
  EXPECT_FALSE(GetUnifiedSidePanel()->GetVisible());
}

TEST_P(LensSidePanelCoordinatorTest,
       CallingRegisterTwiceOpensNewUrlAndLogsAction) {
  base::UserActionTester user_action_tester;

  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
  lens_side_panel_coordinator_->RegisterEntryAndShow(content::OpenURLParams(
      GURL(kLensAlternateHomepageURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false));

  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kLens);
  EXPECT_EQ(2, user_action_tester.GetActionCount(kLensQueryAction));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kLensQuerySidePanelClosedAction));
  EXPECT_EQ(
      1, user_action_tester.GetActionCount(kLensQuerySidePanelOpenLensAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensQueryFollowupAction));
}

TEST_P(LensSidePanelCoordinatorTest, SwitchToDifferentItemTriggersHideEvent) {
  base::UserActionTester user_action_tester;
  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  GetSidePanelCoordinator()->Show(SidePanelEntry::Id::kBookmarks);

  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kBookmarks);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kLensQuerySidePanelClosedAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensEntryHiddenAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensEntryShownAction));
}

TEST_P(LensSidePanelCoordinatorTest, SwitchBackToLensTriggersShowEvent) {
  base::UserActionTester user_action_tester;
  lens_side_panel_coordinator_->RegisterEntryAndShow(
      content::OpenURLParams(GURL(kLensHomepageURL), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));

  GetSidePanelCoordinator()->Show(SidePanelEntry::Id::kBookmarks);
  GetSidePanelCoordinator()->Show(SidePanelEntry::Id::kLens);

  EXPECT_TRUE(GetUnifiedSidePanel()->GetVisible());
  EXPECT_EQ(GetSidePanelCoordinator()
                ->GetCurrentSidePanelEntryForTesting()
                ->key()
                .id(),
            SidePanelEntry::Id::kLens);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensQueryAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kNewLensQueryAction));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(kLensQuerySidePanelClosedAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kLensEntryHiddenAction));
  EXPECT_EQ(2, user_action_tester.GetActionCount(kLensEntryShownAction));
}

INSTANTIATE_TEST_SUITE_P(All, LensSidePanelCoordinatorTest, ::testing::Bool());

}  // namespace
