// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/mock_vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace {

class TestOrganizerPanelView : public OrganizerPanelView {
 public:
  explicit TestOrganizerPanelView(BrowserWindowInterface& browser)
      : OrganizerPanelView(browser) {}
  ~TestOrganizerPanelView() override = default;
};

}  // namespace

class OrganizerPanelControllerTest : public testing::Test {
 public:
  OrganizerPanelControllerTest() = default;
  ~OrganizerPanelControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    WebUIContentsPreloadManager::GetInstance()->DisableNavigationForTesting();

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(&profile_));

    animation_controller_ = std::make_unique<BrowserAnimationController>(
        mock_browser_window_interface_);
    animation_controller_->AddAnimationProvider(
        std::make_unique<OrganizerPanelAnimations>());

    vertical_tab_strip_controller_ =
        std::make_unique<tabs::test::MockVerticalTabStripStateController>(
            mock_browser_window_interface_);
    EXPECT_CALL(*vertical_tab_strip_controller_, ShouldDisplayVerticalTabs)
        .WillRepeatedly(testing::Return(false));

    // Action items like ToggleOrganizerPanel are tested in interactive ui
    // tests.
    controller_ = std::make_unique<OrganizerPanelController>(
        mock_browser_window_interface_, /*root_action_item=*/nullptr);
    panel_view_ = std::make_unique<TestOrganizerPanelView>(
        mock_browser_window_interface_);
    controller_->RegisterPanelView(panel_view_.get());
  }

  void TearDown() override {
    controller_->UnregisterPanelView(panel_view_.get());
    panel_view_.reset();
    controller_.reset();
    vertical_tab_strip_controller_.reset();
    animation_controller_.reset();
    WebUIContentsPreloadManager::GetInstance()->ReenableNavigationForTesting();
    testing::Test::TearDown();
  }

  OrganizerPanelController* controller() { return controller_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BrowserAnimationController> animation_controller_;
  std::unique_ptr<tabs::test::MockVerticalTabStripStateController>
      vertical_tab_strip_controller_;
  std::unique_ptr<OrganizerPanelController> controller_;
  std::unique_ptr<TestOrganizerPanelView> panel_view_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(OrganizerPanelControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelControllerTest, OrganizerPanelEnabled) {
  base::UserActionTester user_action_tester;

  controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(1, user_action_tester.GetActionCount("OrganizerPanel.Open"));
  EXPECT_EQ(0, user_action_tester.GetActionCount("OrganizerPanel.Close"));

  controller()->SetOrganizerVisible(false);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(1, user_action_tester.GetActionCount("OrganizerPanel.Open"));
  EXPECT_EQ(1, user_action_tester.GetActionCount("OrganizerPanel.Close"));
}

TEST_F(OrganizerPanelControllerTest, Subscription) {
  UNCALLED_MOCK_CALLBACK(OrganizerPanelController::StateChangedCallback,
                         callback);
  const auto subscription =
      controller()->RegisterOnStateChanged(callback.Get());

  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->SetOrganizerVisible(true));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  // Setting to same value should not trigger a notification.
  controller()->SetOrganizerVisible(true);
}

TEST_F(OrganizerPanelControllerTest, LazyWebContentsInitializationAndReuse) {
  EXPECT_EQ(controller()->web_contents(), nullptr);
  EXPECT_EQ(controller()->site_engagement_helper(), nullptr);
  controller()->SetOrganizerVisible(true);
  content::WebContents* const web_contents = controller()->web_contents();
  EXPECT_NE(web_contents, nullptr);
  EXPECT_NE(controller()->site_engagement_helper(), nullptr);

  controller()->SetOrganizerVisible(false);
  EXPECT_EQ(controller()->web_contents(), web_contents);
  EXPECT_NE(controller()->site_engagement_helper(), nullptr);

  controller()->SetOrganizerVisible(true);
  EXPECT_EQ(controller()->web_contents(), web_contents);
  EXPECT_NE(controller()->site_engagement_helper(), nullptr);
}

TEST_F(OrganizerPanelControllerTest, ResizingBlocksOpenAndMetrics) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*vertical_tab_strip_controller_, IsResizing())
      .WillOnce(testing::Return(true));

  controller()->SetOrganizerVisible(true);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Tabs.Organizer.Section."),
      testing::IsEmpty());
}

TEST_F(OrganizerPanelControllerTest, RecordsSectionStateMetricsOnOpen) {
  base::HistogramTester histogram_tester;

  controller()->SetOrganizerVisible(true);
  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.Section.OpenTabs.ShowAllStateOnOpen", false, 1);
  EXPECT_THAT(
      histogram_tester.GetTotalCountsForPrefix("Tabs.Organizer.Section."),
      testing::UnorderedElementsAre(
          testing::Pair("Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen",
                        1),
          testing::Pair("Tabs.Organizer.Section.OpenTabs.ShowAllStateOnOpen",
                        1),
          testing::Pair(
              "Tabs.Organizer.Section.RecentlyClosed.ExpandedStateOnOpen", 1),
          testing::Pair(
              "Tabs.Organizer.Section.RecentlyClosed.ShowAllStateOnOpen", 1),
          testing::Pair("Tabs.Organizer.Section.TabGroups.ExpandedStateOnOpen",
                        1),
          testing::Pair("Tabs.Organizer.Section.TabGroups.ShowAllStateOnOpen",
                        1)));

  // Re-opening with updated prefs records the new values without double-logging
  // on redundant show or close calls.
  controller()->SetOrganizerVisible(true);
  controller()->SetOrganizerVisible(false);

  organizer::SetSectionExpanded(*profile_.GetPrefs(),
                                organizer::SectionId::kOpenTabs, false);
  organizer::SetSectionShowAll(*profile_.GetPrefs(),
                               organizer::SectionId::kOpenTabs, true);

  controller()->SetOrganizerVisible(true);
  histogram_tester.ExpectBucketCount(
      "Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen", false, 1);
  histogram_tester.ExpectBucketCount(
      "Tabs.Organizer.Section.OpenTabs.ShowAllStateOnOpen", true, 1);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen", 2);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.OpenTabs.ShowAllStateOnOpen", 2);
}

TEST_F(OrganizerPanelControllerTest,
       CrossDeviceTabsMetricsRequireFeatureAndSignIn) {
  base::HistogramTester histogram_tester;

  // Feature enabled while signed out does not record CrossDeviceTabs.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({organizer_panel::kOrganizerPanel,
                                   organizer_panel::kOrganizerPanelForeignTabs},
                                  {});
    controller()->SetOrganizerVisible(true);
    controller()->SetOrganizerVisible(false);
  }

  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(&profile_), "user@example.com",
      signin::ConsentLevel::kSignin);

  // Feature disabled while signed in does not record CrossDeviceTabs.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
    controller()->SetOrganizerVisible(true);
    controller()->SetOrganizerVisible(false);
  }

  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.CrossDeviceTabs.ExpandedStateOnOpen", 0);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.CrossDeviceTabs.ShowAllStateOnOpen", 0);

  // Feature enabled while signed in records CrossDeviceTabs.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({organizer_panel::kOrganizerPanel,
                                 organizer_panel::kOrganizerPanelForeignTabs},
                                {});
  controller()->SetOrganizerVisible(true);
  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.Section.CrossDeviceTabs.ExpandedStateOnOpen", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.Section.CrossDeviceTabs.ShowAllStateOnOpen", false, 1);
}

TEST_F(OrganizerPanelControllerTest,
       IncognitoSkipsRecentlyClosedAndCrossDeviceMetrics) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({organizer_panel::kOrganizerPanel,
                                 organizer_panel::kOrganizerPanelForeignTabs},
                                {});
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(&profile_), "user@example.com",
      signin::ConsentLevel::kSignin);

  Profile* const incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(mock_browser_window_interface_, GetProfile())
      .WillRepeatedly(testing::Return(incognito_profile));

  base::HistogramTester histogram_tester;
  controller()->SetOrganizerVisible(true);

  histogram_tester.ExpectUniqueSample(
      "Tabs.Organizer.Section.OpenTabs.ExpandedStateOnOpen", true, 1);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.RecentlyClosed.ExpandedStateOnOpen", 0);
  histogram_tester.ExpectTotalCount(
      "Tabs.Organizer.Section.CrossDeviceTabs.ExpandedStateOnOpen", 0);
}

// Test suite that ensures that the panel gets parented to the tray and the
// vertical tab strip if the tab strip is enabled.
class OrganizerPanelControllerMovePanelTest
    : public organizer_panel::test::OrganizerPanelTestBase {
 public:
  OrganizerPanelControllerMovePanelTest() {
    feature_list_.InitAndEnableFeature(organizer_panel::kOrganizerPanel);
  }
  ~OrganizerPanelControllerMovePanelTest() override = default;

  virtual bool StartInVerticalTabStrip() const { return true; }

  void BeforeSetPanel() override {
    SetTabstripProperties(StartInVerticalTabStrip(), false, true,
                          organizer_panel::kOrganizerPanelMinWidth);
  }

  auto ConfigureVerticalTabStrip(
      bool enabled,
      bool collapsed = false,
      bool expand_on_hover = true,
      int uncollapsed_width = organizer_panel::kOrganizerPanelMinWidth) {
    return Do([=, this]() {
             vertical_tab_strip_controller_->NotifyModeWillChange();
             SetTabstripProperties(enabled, collapsed, expand_on_hover,
                                   uncollapsed_width);
             vertical_tab_strip_controller_->NotifyModeChanged();
           })
        .SetDescription("ConfigureVerticalTabstrip()");
  }

  auto ExpectPanel(bool in_tab_strip) {
    auto steps = Steps(
        WaitForShow(kOrganizerPanelElementId),
        CheckView(
            kOrganizerPanelElementId,
            [this, in_tab_strip](views::View* view) {
              return (in_tab_strip ? static_cast<views::View*>(tab_strip())
                                   : tray_view())
                  ->Contains(view);
            },
            true),
        CheckResult(
            [this]() {
              return state_controller_
                  ->IsOrganizerPanelEmbeddedInVerticalTabStrip();
            },
            in_tab_strip));
    AddDescriptionPrefix(steps, "ExpectPanel");
    return steps;
  }

  views::View* GetExpectedParent(bool expect_tab_strip) {
    if (expect_tab_strip) {
      return tab_strip();
    }
    return tray_view();
  }

 private:
  void SetTabstripProperties(bool enabled,
                             bool collapsed,
                             bool expand_on_hover,
                             int uncollapsed_width) {
    EXPECT_CALL(*vertical_tab_strip_controller_, ShouldDisplayVerticalTabs)
        .WillRepeatedly(testing::Return(enabled));
    EXPECT_CALL(*vertical_tab_strip_controller_, IsCollapsed)
        .WillRepeatedly(testing::Return(collapsed));
    EXPECT_CALL(*vertical_tab_strip_controller_, IsExpandOnHoverEnabled)
        .WillRepeatedly(testing::Return(expand_on_hover));
    EXPECT_CALL(*vertical_tab_strip_controller_, GetUncollapsedWidth)
        .WillRepeatedly(testing::Return(uncollapsed_width));
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OrganizerPanelControllerMovePanelTest, HorizontalTabStrip) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(false), TogglePanel(),
                  SetAnimationValue(1.0), ExpectPanel(false));
}

TEST_F(OrganizerPanelControllerMovePanelTest,
       LargeUncollapsedVerticalTabStrip) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(true), TogglePanel(),
                  SetAnimationValue(1.0), ExpectPanel(true));
}

TEST_F(OrganizerPanelControllerMovePanelTest,
       SmallUncollapsedVerticalTabStrip) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(
                      true, false, true,
                      organizer_panel::kOrganizerPanelMinWidth -
                          (organizer_panel::kOrganizerPanelMinOverlap + 10)),
                  TogglePanel(), SetAnimationValue(1.0), ExpectPanel(false));
}

TEST_F(OrganizerPanelControllerMovePanelTest,
       CollapsedVerticalTabStripWithExpandOnHover) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(true, true, true), TogglePanel(),
                  SetAnimationValue(1.0), ExpectPanel(true));
}

TEST_F(OrganizerPanelControllerMovePanelTest,
       CollapsedVerticalTabStripWithoutExpandOnHover) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(true, true, false), TogglePanel(),
                  SetAnimationValue(1.0), ExpectPanel(false));
}

TEST_F(OrganizerPanelControllerMovePanelTest,
       NotifiesStateChangedWhenLocationChangesWhileHidden) {
  ASSERT_FALSE(state_controller_->IsOrganizerPanelVisible());
  ASSERT_EQ(OrganizerPanelLocation::kVerticalTabStrip,
            state_controller_->GetCurrentOrganizerPanelLocation());

  UNCALLED_MOCK_CALLBACK(OrganizerPanelController::StateChangedCallback,
                         callback);
  const auto subscription =
      state_controller_->RegisterOnStateChanged(callback.Get());

  EXPECT_CALL_IN_SCOPE(callback, Run, {
    vertical_tab_strip_controller_->NotifyModeWillChange();
    EXPECT_CALL(*vertical_tab_strip_controller_, ShouldDisplayVerticalTabs)
        .WillRepeatedly(testing::Return(false));
    vertical_tab_strip_controller_->NotifyModeChanged();
  });
  EXPECT_EQ(OrganizerPanelLocation::kOrganizerTray,
            state_controller_->GetCurrentOrganizerPanelLocation());

  // Re-notifying with the same location should not trigger another callback.
  vertical_tab_strip_controller_->NotifyModeChanged();
}

class OrganizerPanelControllerConfigurationTest
    : public OrganizerPanelControllerMovePanelTest,
      public testing::WithParamInterface<bool> {
 public:
  OrganizerPanelControllerConfigurationTest() = default;
  ~OrganizerPanelControllerConfigurationTest() override = default;

  bool StartInVerticalTabStrip() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         OrganizerPanelControllerConfigurationTest,
                         testing::Bool(),
                         [](testing::TestParamInfo<bool> v) {
                           return v.param ? "Vertical" : "Horizontal";
                         });

TEST_P(OrganizerPanelControllerConfigurationTest, PanelStartsInCorrectPlace) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId), TogglePanel(),
                  SetAnimationValue(1.0),
                  ExpectPanel(StartInVerticalTabStrip()));
}

TEST_P(OrganizerPanelControllerConfigurationTest, PanelMoved) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(!StartInVerticalTabStrip()),
                  TogglePanel(), SetAnimationValue(1.0),
                  ExpectPanel(!StartInVerticalTabStrip()));
}
