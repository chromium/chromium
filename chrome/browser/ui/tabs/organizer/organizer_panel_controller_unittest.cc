// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <memory>

#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/tabs/mock_vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_test.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class OrganizerPanelControllerTest : public testing::Test {
 public:
  OrganizerPanelControllerTest() = default;
  ~OrganizerPanelControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

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
  }

  void TearDown() override {
    controller_.reset();
    vertical_tab_strip_controller_.reset();
    animation_controller_.reset();
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
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(OrganizerPanelControllerTest, Initial) {
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
}

TEST_F(OrganizerPanelControllerTest, OrganizerPanelEnabled) {
  controller()->SetOrganizerVisible(true);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  controller()->SetOrganizerVisible(false);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(OrganizerPanelControllerTest, ExtensionOpenToggleClose) {
  UNCALLED_MOCK_CALLBACK(OrganizerPanelController::StateChangedCallback,
                         callback);
  const auto subscription =
      controller()->RegisterOnStateChanged(callback.Get());

  const extensions::ExtensionId kExt1 = "abcdefghijklmnopabcdefghijklmnop";
  const extensions::ExtensionId kExt2 = "ponmlkjihgfedcbaponmlkjihgfedcba";

  // Opening for extension opens panel and sets active extension ID.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->OpenForExtension(kExt1));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);

  // Calling OpenForExtension with the same extension when already open
  // should not notify again.
  controller()->OpenForExtension(kExt1);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());

  // Opening for another extension keeps it open and updates active extension.
  controller()->OpenForExtension(kExt2);
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt2);

  // Toggling same extension closes the panel.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->ToggleForExtension(kExt2));
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());

  // Toggling while closed opens it.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->ToggleForExtension(kExt1));
  EXPECT_TRUE(controller()->IsOrganizerPanelVisible());
  EXPECT_EQ(controller()->active_extension_id(), kExt1);

  // Closing matching extension closes the panel.
  EXPECT_CALL_IN_SCOPE(callback, Run, controller()->CloseForExtension(kExt1));
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());
}

TEST_F(OrganizerPanelControllerTest, IncognitoRejectsUnauthorizedExtension) {
  Profile* const incognito_profile =
      profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(mock_browser_window_interface_, GetProfile())
      .WillRepeatedly(testing::Return(incognito_profile));

  const extensions::ExtensionId kExt = "abcdefghijklmnopabcdefghijklmnop";

  // Opening for an unauthorized extension in incognito should do nothing.
  controller()->OpenForExtension(kExt);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());

  // Toggling while closed also should do nothing.
  controller()->ToggleForExtension(kExt);
  EXPECT_FALSE(controller()->IsOrganizerPanelVisible());
  EXPECT_FALSE(controller()->active_extension_id().has_value());
}
#endif

// Test suite that ensures that the panel gets parented to the tray and the
// vertical tab strip if the tab strip is enabled.
class OrganizerPanelControllerMovePanelTest
    : public organizer_panel::test::OrganizerPanelTestBase {
 public:
  explicit OrganizerPanelControllerMovePanelTest(bool enable = true)
      : flag_enabled_(enable) {
    feature_list_.InitAndEnableFeatureWithParameters(
        organizer_panel::kOrganizerPanel,
        {{"OrganizerPanelInVerticalTabStrip", enable ? "true" : "false"}});
  }
  ~OrganizerPanelControllerMovePanelTest() override = default;

  bool flag_enabled() const { return flag_enabled_; }

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
            true));
    AddDescriptionPrefix(steps, "ExpectPanel");
    return steps;
  }

  views::View* GetExpectedParent(bool expect_tab_strip) {
    if (flag_enabled() && expect_tab_strip) {
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

  const bool flag_enabled_;
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

class OrganizerPanelControllerConfigurationTest
    : public OrganizerPanelControllerMovePanelTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  OrganizerPanelControllerConfigurationTest()
      : OrganizerPanelControllerMovePanelTest(std::get<1>(GetParam())) {}
  ~OrganizerPanelControllerConfigurationTest() override = default;

  bool StartInVerticalTabStrip() const override {
    return std::get<0>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         OrganizerPanelControllerConfigurationTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         [](testing::TestParamInfo<std::tuple<bool, bool>> v) {
                           return base::StringPrintf("tab_strip_%d_flag_%d",
                                                     std::get<0>(v.param),
                                                     std::get<1>(v.param));
                         });

TEST_P(OrganizerPanelControllerConfigurationTest, PanelStartsInCorrectPlace) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId), TogglePanel(),
                  SetAnimationValue(1.0),
                  ExpectPanel(flag_enabled() && StartInVerticalTabStrip()));
}

TEST_P(OrganizerPanelControllerConfigurationTest, PanelMoved) {
  RunTestSequence(EnsureNotPresent(kOrganizerPanelElementId),
                  ConfigureVerticalTabStrip(!StartInVerticalTabStrip()),
                  TogglePanel(), SetAnimationValue(1.0),
                  ExpectPanel(flag_enabled() && !StartInVerticalTabStrip()));
}
