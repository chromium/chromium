// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"

#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

namespace {
constexpr int kUncollapsedWidth1 = 100;
constexpr int kUncollapsedWidth2 = 200;
constexpr int kSessionIDValue = 123;
}  // namespace

class VerticalTabStripStateControllerTest : public testing::Test {
 public:
  VerticalTabStripStateControllerTest() = default;
  ~VerticalTabStripStateControllerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kVerticalTabsEnabled, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
    SessionID test_session_id = SessionID::FromSerializedValue(kSessionIDValue);

    // Action items like CollapseActionItem are tested in interactive ui tests.
    controller_ = std::make_unique<VerticalTabStripStateController>(
        &pref_service_, /*root_action_item=*/nullptr,
        /*session_service*/ nullptr, test_session_id);
  }

  void TearDown() override {
    controller_.reset();
    testing::Test::TearDown();
  }

  VerticalTabStripStateController* controller() { return controller_.get(); }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }

 private:
  std::unique_ptr<VerticalTabStripStateController> controller_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(VerticalTabStripStateControllerTest, Initial) {
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_FALSE(controller()->IsCollapsed());
  EXPECT_EQ(0, controller()->GetUncollapsedWidth());
}

TEST_F(VerticalTabStripStateControllerTest, VerticalTabsEnabled) {
  controller()->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));

  controller()->SetVerticalTabsEnabled(false);
  EXPECT_FALSE(controller()->ShouldDisplayVerticalTabs());
  EXPECT_FALSE(pref_service()->GetBoolean(prefs::kVerticalTabsEnabled));
}

TEST_F(VerticalTabStripStateControllerTest, Collapsed) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, VerticalTabStripStateController* controller) {
        (*call_count)++;
        EXPECT_TRUE(controller->IsCollapsed());
      },
      &call_count));

  controller()->SetCollapsed(true);
  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetCollapsed(true);
  EXPECT_EQ(1, call_count);
}

TEST_F(VerticalTabStripStateControllerTest, UncollapsedWidth) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, VerticalTabStripStateController* controller) {
        (*call_count)++;
        EXPECT_EQ(kUncollapsedWidth1, controller->GetUncollapsedWidth());
      },
      &call_count));

  controller()->SetUncollapsedWidth(kUncollapsedWidth1);
  EXPECT_EQ(kUncollapsedWidth1, controller()->GetUncollapsedWidth());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetUncollapsedWidth(kUncollapsedWidth1);
  EXPECT_EQ(1, call_count);
}

TEST_F(VerticalTabStripStateControllerTest, State) {
  int call_count = 0;
  auto subscription = controller()->RegisterOnStateChanged(base::BindRepeating(
      [](int* call_count, VerticalTabStripStateController* controller) {
        (*call_count)++;
        EXPECT_TRUE(controller->IsCollapsed());
        EXPECT_EQ(kUncollapsedWidth2, controller->GetUncollapsedWidth());
      },
      &call_count));

  VerticalTabStripState state;
  state.collapsed = true;
  state.uncollapsed_width = kUncollapsedWidth2;
  controller()->SetState(state);

  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_EQ(kUncollapsedWidth2, controller()->GetUncollapsedWidth());
  EXPECT_EQ(1, call_count);

  // Setting to same value should not trigger a notification.
  controller()->SetState(state);
  EXPECT_EQ(1, call_count);
}

}  // namespace tabs
