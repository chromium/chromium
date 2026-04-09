// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

#include <optional>

#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/test_vertical_tab_strip_state_controller_delegate.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace tabs {

namespace {
constexpr int kUncollapsedWidth1 = 100;
constexpr int kSessionIDValue = 123;
}  // namespace

class VerticalTabStripStateControllerTest : public testing::Test {
 public:
  VerticalTabStripStateControllerTest() = default;
  ~VerticalTabStripStateControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    feature_list_.InitWithFeatures(
        /* enabled_features */ {tabs::kVerticalTabs,
                                tabs::kVerticalTabsExpandOnHover},
        /* disabled_features */ {});
    tabs::RegisterProfilePrefs(pref_service_.registry());
    SessionID test_session_id = SessionID::FromSerializedValue(kSessionIDValue);

    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    // Action items like CollapseActionItem are tested in interactive ui tests.
    controller_ = std::make_unique<VerticalTabStripStateController>(
        &mock_browser_window_interface_, &pref_service_,
        /*root_action_item=*/nullptr,
        /*session_service=*/nullptr, test_session_id,
        /*restored_state_collapsed=*/std::nullopt,
        /*restored_state_uncollapsed_width=*/std::nullopt);
    delegate_ = std::make_unique<TestVerticalTabStripStateControllerDelegate>();
    controller_->SetDelegate(delegate_.get());
  }

  void TearDown() override {
    controller_->SetDelegate(nullptr);
    controller_.reset();
    testing::Test::TearDown();
  }

  VerticalTabStripStateController* controller() { return controller_.get(); }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestVerticalTabStripStateControllerDelegate> delegate_;
  std::unique_ptr<VerticalTabStripStateController> controller_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

TEST_F(VerticalTabStripStateControllerTest, Initial) {
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_FALSE(controller()->IsCollapsed());
  EXPECT_EQ(kVerticalTabStripDefaultUncollapsedWidth,
            controller()->GetUncollapsedWidth());
}

TEST_F(VerticalTabStripStateControllerTest, VerticalTabsEnabled) {
  auto subscription = controller()->RegisterOnModeChanged(base::BindRepeating(
      [](bool display_vertical_tabs,
         VerticalTabStripStateController* controller) {
        EXPECT_EQ(display_vertical_tabs,
                  controller->ShouldDisplayVerticalTabs());
      },
      true));

  controller()->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));

  subscription = controller()->RegisterOnModeChanged(base::BindRepeating(
      [](bool display_vertical_tabs,
         VerticalTabStripStateController* controller) {
        EXPECT_EQ(display_vertical_tabs,
                  controller->ShouldDisplayVerticalTabs());
      },
      false));
  controller()->SetVerticalTabsEnabled(false);
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
}

TEST_F(VerticalTabStripStateControllerTest, FeatureDisabled) {
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(tabs::kVerticalTabs);

  controller()->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
  // Even if pref is true, ShouldDisplayVerticalTabs should be false if feature
  // is disabled.
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
}

TEST_F(VerticalTabStripStateControllerTest, VerticalTabsEnabledFirstTime) {
  base::UserActionTester user_action_tester;
  ASSERT_FALSE(
      pref_service()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime));
  ASSERT_EQ(0,
            user_action_tester.GetActionCount("VerticalTabs_EnabledFirstTime"));

  controller()->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("VerticalTabs_EnabledFirstTime"));

  controller()->SetVerticalTabsEnabled(false);
  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("VerticalTabs_EnabledFirstTime"));

  controller()->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabledFirstTime));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("VerticalTabs_EnabledFirstTime"));
}

TEST_F(VerticalTabStripStateControllerTest, Collapsed) {
  int changed_call_count = 0;
  auto changed_subscription =
      controller()->RegisterOnCollapseChanged(base::BindRepeating(
          [](int* changed_call_count,
             tabs::VerticalTabStripCollapseState state) {
            (*changed_call_count)++;
            EXPECT_TRUE(state !=
                        tabs::VerticalTabStripCollapseState::kExpanded);
          },
          &changed_call_count));

  controller()->RequestCollapse(true);
  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_EQ(1, changed_call_count);

  controller()->RequestCollapse(true);
  // Setting to same value should not trigger a notification.
  EXPECT_EQ(1, changed_call_count);
}

TEST_F(VerticalTabStripStateControllerTest, UncollapsedWidth) {
  int call_count = 0;
  auto subscription =
      controller()->RegisterOnCollapseChanged(base::BindRepeating(
          [](int* call_count, VerticalTabStripStateController* controller,
             tabs::VerticalTabStripCollapseState state) {
            (*call_count)++;
            EXPECT_EQ(kUncollapsedWidth1, controller->GetUncollapsedWidth());
          },
          &call_count, controller()));

  controller()->SetUncollapsedWidth(kUncollapsedWidth1);
  EXPECT_EQ(kUncollapsedWidth1, controller()->GetUncollapsedWidth());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetUncollapsedWidth(kUncollapsedWidth1);
  EXPECT_EQ(1, call_count);
}

TEST_F(VerticalTabStripStateControllerTest, ExpandOnHover) {
  controller()->SetExpandOnHoverEnabled(true);
  EXPECT_TRUE(controller()->IsExpandOnHoverEnabled());
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled));

  controller()->SetExpandOnHoverEnabled(false);
  EXPECT_FALSE(controller()->IsExpandOnHoverEnabled());
  EXPECT_FALSE(
      pref_service()->GetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled));
}

TEST_F(VerticalTabStripStateControllerTest, ExpandOnHoverEnabledChanged) {
  int call_count = 0;
  bool last_enabled = false;
  auto subscription =
      controller()->RegisterOnExpandOnHoverEnabledChanged(base::BindRepeating(
          [](int* call_count, bool* last_enabled, bool enabled) {
            (*call_count)++;
            *last_enabled = enabled;
          },
          &call_count, &last_enabled));

  controller()->SetExpandOnHoverEnabled(true);
  EXPECT_TRUE(last_enabled);
  EXPECT_EQ(1, call_count);

  controller()->SetExpandOnHoverEnabled(false);
  EXPECT_FALSE(last_enabled);
  EXPECT_EQ(2, call_count);

  // Setting to same value via pref service should also trigger notification
  // because the controller observes the pref change.
  pref_service()->SetBoolean(prefs::kVerticalTabsExpandOnHoverEnabled, true);
  EXPECT_TRUE(last_enabled);
  EXPECT_EQ(3, call_count);
}

TEST_F(VerticalTabStripStateControllerTest, ImmersiveModeLock) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnModeChanged(base::BindRepeating(
      [](int* call_count, VerticalTabStripStateController* controller) {
        (*call_count)++;
      },
      &call_count));

  // Initially disabled.
  ASSERT_FALSE(controller()->ShouldDisplayVerticalTabs());

  // Take a lock.
  std::unique_ptr<VerticalTabStripStateController::ScopedEnableStateLock> lock =
      controller()->GetEnableStateLock();

  // Enable vertical tabs via preference.
  pref_service()->SetBoolean(prefs::kVerticalTabsEnabled, true);

  // Verify that the state has NOT changed and no notification was sent.
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_EQ(0, call_count);

  // Release the lock.
  lock.reset();

  // Verify that the state HAS changed and notification was sent.
  EXPECT_TRUE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_EQ(1, call_count);

  // Take lock again.
  lock = controller()->GetEnableStateLock();

  // Disable vertical tabs via preference.
  pref_service()->SetBoolean(prefs::kVerticalTabsEnabled, false);

  // Verify state hasn't changed.
  EXPECT_TRUE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_EQ(1, call_count);

  // Release lock.
  lock.reset();

  // Verify state changed.
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_EQ(2, call_count);
}

TEST_F(VerticalTabStripStateControllerTest, VerifyRecentlyUsedPrefs) {
  // Initially, recently used prefs should be default.
  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kVerticalTabsCollapsedState));
  EXPECT_EQ(kVerticalTabStripDefaultUncollapsedWidth,
            pref_service()->GetInteger(prefs::kVerticalTabsUncollapsedWidth));

  // Change state.
  controller()->RequestCollapse(true);
  controller()->SetUncollapsedWidth(kUncollapsedWidth1);

  // Verify recently used prefs are updated.
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsCollapsedState));
  EXPECT_EQ(kUncollapsedWidth1,
            pref_service()->GetInteger(prefs::kVerticalTabsUncollapsedWidth));
}

}  // namespace tabs
