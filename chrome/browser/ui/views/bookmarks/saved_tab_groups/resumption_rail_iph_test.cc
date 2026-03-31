// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_delegate_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace {
constexpr int kNumTabGroupsToForceOverflow = 30;
}  // namespace

class MockProjectsPanelStateController : public ProjectsPanelStateController {
 public:
  explicit MockProjectsPanelStateController(
      BrowserWindowInterface* browser_window,
      bool* can_show_aim,
      bool* can_show_gemini)
      : ProjectsPanelStateController(browser_window,
                                     /*root_action_item=*/nullptr,
                                     /*aim_eligibility_service=*/nullptr,
                                     /*glic_enabling=*/nullptr),
        can_show_aim_(can_show_aim),
        can_show_gemini_(can_show_gemini) {}

  bool CanShowAimThreads() override { return *can_show_aim_; }
  bool CanShowGeminiThreads() override { return *can_show_gemini_; }

 private:
  raw_ptr<bool> can_show_aim_;
  raw_ptr<bool> can_show_gemini_;
};

class ResumptionRailPromoTest
    : public VerticalTabsBrowserTestMixin<InteractiveFeaturePromoTest> {
 public:
  ResumptionRailPromoTest()
      : VerticalTabsBrowserTestMixin(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHResumptionRailFeature,
             feature_engagement::kIPHReadingListDiscoveryFeature,
             tab_groups::kProjectsPanel, tabs::kVerticalTabs})) {}

  ~ResumptionRailPromoTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveFeaturePromoTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    VerticalTabsBrowserTestMixin::SetUpInProcessBrowserTestFixture();
    projects_panel_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting<MockProjectsPanelStateController>(
                base::BindRepeating(
                    [](bool* can_show_aim, bool* can_show_gemini,
                       BrowserWindowInterface& browser)
                        -> std::unique_ptr<MockProjectsPanelStateController> {
                      return std::make_unique<MockProjectsPanelStateController>(
                          &browser, can_show_aim, can_show_gemini);
                    },
                    base::Unretained(&can_show_aim_),
                    base::Unretained(&can_show_gemini_)));
  }

  void TearDownInProcessBrowserTestFixture() override {
    projects_panel_override_ = ui::UserDataFactory::ScopedOverride();
    VerticalTabsBrowserTestMixin::TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin::SetUpOnMainThread();
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
      // Force a layout to ensure the overflow button is shown.
      RunScheduledLayouts();
    });
  }

 protected:
  bool can_show_aim_ = false;
  bool can_show_gemini_ = false;

 private:
  ui::UserDataFactory::ScopedOverride projects_panel_override_;
};

struct ResumptionRailPromoBodyTestCase {
  bool can_show_aim;
  bool can_show_gemini;
  int expected_string_id;
  std::string test_name;
};

class ResumptionRailPromoBodyTest
    : public ResumptionRailPromoTest,
      public testing::WithParamInterface<ResumptionRailPromoBodyTestCase> {
 public:
  void SetUpOnMainThread() override {
    can_show_aim_ = GetParam().can_show_aim;
    can_show_gemini_ = GetParam().can_show_gemini;
    ResumptionRailPromoTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_P(ResumptionRailPromoBodyTest, CheckPromoString) {
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
                  // Check the IPH body text.
                  CheckViewProperty(
                      user_education::HelpBubbleView::kBodyTextIdForTesting,
                      &views::Label::GetText,
                      l10n_util::GetStringUTF16(GetParam().expected_string_id)),
                  // Click the new Projects button to dismiss the promo.
                  PressButton(kVerticalTabStripProjectsButtonElementId),
                  // Should hide the Everything menu button.
                  WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ResumptionRailPromoBodyTest,
    testing::Values(
        ResumptionRailPromoBodyTestCase{
            .can_show_aim = false,
            .can_show_gemini = false,
            .expected_string_id = IDS_RESUMPTION_RAIL_IPH_BODY_NO_THREADS,
            .test_name = "NoThreads"},
        ResumptionRailPromoBodyTestCase{
            .can_show_aim = true,
            .can_show_gemini = false,
            .expected_string_id = IDS_RESUMPTION_RAIL_IPH_BODY_ONLY_AI_MODE,
            .test_name = "OnlyAim"},
        ResumptionRailPromoBodyTestCase{
            .can_show_aim = false,
            .can_show_gemini = true,
            .expected_string_id = IDS_RESUMPTION_RAIL_IPH_BODY_ONLY_GEMINI,
            .test_name = "OnlyGemini"},
        ResumptionRailPromoBodyTestCase{
            .can_show_aim = true,
            .can_show_gemini = true,
            .expected_string_id = IDS_RESUMPTION_RAIL_IPH_BODY,
            .test_name = "Both"}),
    [](const testing::TestParamInfo<ResumptionRailPromoBodyTestCase>& info) {
      return info.param.test_name;
    });

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

// TODO(crbug.com/497983821): Flaky on ChromeOS and Linux
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_HideOverflowButtonWithinGracePeriodIfNewProfile \
  DISABLED_HideOverflowButtonWithinGracePeriodIfNewProfile
#else
#define MAYBE_HideOverflowButtonWithinGracePeriodIfNewProfile \
  HideOverflowButtonWithinGracePeriodIfNewProfile
#endif
IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest,
                       MAYBE_HideOverflowButtonWithinGracePeriodIfNewProfile) {
  auto* service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
  service->user_education_storage_service()
      .set_profile_creation_time_for_testing(base::Time::Now());

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kVerticalTabStripProjectsButtonElementId),
      // Add enough groups to force the overflow button to appear.
      AddTabGroupsToForceOverflow(), WaitForShow(kSavedTabGroupBarElementId),
      // The overflow button should be hidden because the profile is new.
      WaitForHide(kSavedTabGroupOverflowButtonElementId),
      // Advance time to pass the grace period for the promo.
      AdvanceTime(user_education::features::GetNewProfileGracePeriod() +
                  base::Days(1)),
      // Still should be hidden.
      WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest,
                       HideOverflowButtonAfterGracePeriodIfNewProfile) {
  auto* service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
  service->user_education_storage_service()
      .set_profile_creation_time_for_testing(base::Time::Now());

  RunTestSequence(
      // Advance time to pass the grace period for the promo.
      AdvanceTime(user_education::features::GetNewProfileGracePeriod() +
                  base::Days(1)),
      WaitForShow(kBookmarkBarElementId),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kVerticalTabStripProjectsButtonElementId),
      // Add enough groups to force the overflow button to appear.
      AddTabGroupsToForceOverflow(), WaitForShow(kSavedTabGroupBarElementId),
      // Everything button should be hidden.
      WaitForHide(kSavedTabGroupOverflowButtonElementId));
}

IN_PROC_BROWSER_TEST_F(ResumptionRailPromoTest,
                       ShowOverflowButtonIfLegacyProfile) {
  auto* service =
      UserEducationServiceFactory::GetForBrowserContext(browser()->profile());
  service->user_education_storage_service()
      .set_profile_creation_time_for_testing(
          base::Time::Now() -
          user_education::features::GetNewProfileGracePeriod() - base::Days(1));

  RunTestSequence(
      WaitForShow(kBookmarkBarElementId),
      Do([this]() { RunScheduledLayouts(); }),
      WaitForShow(kVerticalTabStripProjectsButtonElementId),
      // Add enough groups to force the overflow button to appear.
      AddTabGroupsToForceOverflow(), WaitForShow(kSavedTabGroupBarElementId),
      // The overflow button should be visible because the profile is legacy.
      CheckView(kSavedTabGroupOverflowButtonElementId,
                [](views::View* view) { return view->GetVisible(); }),
      PressButton(kSavedTabGroupOverflowButtonElementId),
      // The IPH should trigger.
      WaitForPromo(feature_engagement::kIPHResumptionRailFeature),
      // Should hide the Everything button.
      WaitForHide(kSavedTabGroupOverflowButtonElementId));
}
