// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_selection_adapter.h"

#include <memory>
#include <optional>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_selection_state.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define ADAPTER_CALL(call)              \
  do {                                  \
    selection_state_adapter_.call;      \
    list_selection_model_adapter_.call; \
  } while (false)

// Test class for TabStripModelSelectionAdapter. This class tests two
// implementations of TabStripModelSelectionAdapter to verify that both have the
// same behavior for the same inputs.
class TabStripModelSelectionAdapterTest : public testing::Test {
 public:
  TabStripModelSelectionAdapterTest()
      : model_(&delegate_, &profile_), selection_state_adapter_(&model_) {}

  void SetUp() override {
    MockBrowserWindowInterface bwi;
    delegate_.SetBrowserWindowInterface(&bwi);
    for (int i = 0; i < 20; ++i) {
      model_.AppendWebContents(
          content::WebContentsTester::CreateTestWebContents(&profile_, nullptr),
          false);
    }
  }

  void TearDown() override { delegate_.SetBrowserWindowInterface(nullptr); }

 protected:
  // Since both adapters have had the same functions called on them, they should
  // both have the expected state and the same state between both.
  void CheckState(const std::string& expected_state) {
    EXPECT_EQ(expected_state,
              SelectionStateToString(list_selection_model_adapter_));
    EXPECT_EQ(SelectionStateToString(selection_state_adapter_),
              SelectionStateToString(list_selection_model_adapter_));
    EXPECT_EQ(selection_state_adapter_.ToListSelectionModel(),
              list_selection_model_adapter_.ToListSelectionModel());
  }

  // Helper for getting the selection state in a human readable format.
  std::string SelectionStateToString(
      const TabStripModelSelectionAdapter& adapter) {
    std::string res = "active=";
    if (adapter.active().has_value()) {
      res += base::ToString(adapter.active().value());
    } else {
      res += "<none>";
    }
    res += " anchor=";
    if (adapter.anchor().has_value()) {
      res += base::ToString(adapter.anchor().value());
    } else {
      res += "<none>";
    }
    res += " selection=";
    const auto& indices = adapter.selected_indices();
    for (auto it = indices.begin(); it != indices.end();) {
      res += base::ToString(*it);
      if (++it != indices.end()) {
        res += " ";
      }
    }
    return res;
  }

  // Since the TabStripModel/WebContents are required for TabInterface ptr
  // ownership the following objects are required to be instantiated
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  TestTabStripModelDelegate delegate_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  TabStripModel model_;

  // The adapters that are being tested.
  TabStripModelSelectionStateAdapter selection_state_adapter_;
  ListSelectionModelAdapter list_selection_model_adapter_;
};

TEST_F(TabStripModelSelectionAdapterTest, InitialState) {
  CheckState("active=<none> anchor=<none> selection=");
  EXPECT_TRUE(selection_state_adapter_.empty());
  EXPECT_TRUE(list_selection_model_adapter_.empty());
}

TEST_F(TabStripModelSelectionAdapterTest, SetSelectedIndex) {
  ADAPTER_CALL(SetSelectedIndex(2));
  CheckState("active=2 anchor=2 selection=2");
  EXPECT_FALSE(selection_state_adapter_.empty());
  EXPECT_FALSE(list_selection_model_adapter_.empty());
}

TEST_F(TabStripModelSelectionAdapterTest, SetSelectedIndexToEmpty) {
  ADAPTER_CALL(SetSelectedIndex(2));
  ADAPTER_CALL(SetSelectedIndex(std::nullopt));
  CheckState("active=<none> anchor=<none> selection=");
  EXPECT_TRUE(selection_state_adapter_.empty());
  EXPECT_TRUE(list_selection_model_adapter_.empty());
}

TEST_F(TabStripModelSelectionAdapterTest, IsSelected) {
  ADAPTER_CALL(SetSelectedIndex(2));
  EXPECT_FALSE(selection_state_adapter_.IsSelected(0));
  EXPECT_FALSE(list_selection_model_adapter_.IsSelected(0));
  EXPECT_TRUE(selection_state_adapter_.IsSelected(2));
  EXPECT_TRUE(list_selection_model_adapter_.IsSelected(2));
}

TEST_F(TabStripModelSelectionAdapterTest, AddIndexToSelected) {
  ADAPTER_CALL(AddIndexToSelection(2));
  CheckState("active=<none> anchor=<none> selection=2");

  ADAPTER_CALL(AddIndexToSelection(4));
  CheckState("active=<none> anchor=<none> selection=2 4");
}

TEST_F(TabStripModelSelectionAdapterTest, AddIndexRangeToSelection) {
  ADAPTER_CALL(AddIndexRangeToSelection(2, 3));
  CheckState("active=<none> anchor=<none> selection=2 3");

  ADAPTER_CALL(AddIndexRangeToSelection(4, 5));
  CheckState("active=<none> anchor=<none> selection=2 3 4 5");

  ADAPTER_CALL(AddIndexRangeToSelection(1, 1));
  CheckState("active=<none> anchor=<none> selection=1 2 3 4 5");
}

TEST_F(TabStripModelSelectionAdapterTest, RemoveIndexFromSelection) {
  ADAPTER_CALL(SetSelectedIndex(2));
  ADAPTER_CALL(AddIndexToSelection(4));
  CheckState("active=2 anchor=2 selection=2 4");

  ADAPTER_CALL(RemoveIndexFromSelection(4));
  CheckState("active=2 anchor=2 selection=2");

  ADAPTER_CALL(RemoveIndexFromSelection(2));
  CheckState("active=2 anchor=2 selection=");
}

TEST_F(TabStripModelSelectionAdapterTest, SetSelectionFromAnchorTo) {
  ADAPTER_CALL(SetSelectedIndex(2));
  ADAPTER_CALL(SetSelectionFromAnchorTo(7));
  CheckState("active=7 anchor=2 selection=2 3 4 5 6 7");

  ADAPTER_CALL(Clear());
  ADAPTER_CALL(SetSelectedIndex(7));
  ADAPTER_CALL(SetSelectionFromAnchorTo(2));
  CheckState("active=2 anchor=7 selection=2 3 4 5 6 7");

  ADAPTER_CALL(Clear());
  ADAPTER_CALL(SetSelectionFromAnchorTo(7));
  CheckState("active=7 anchor=7 selection=7");
}

TEST_F(TabStripModelSelectionAdapterTest, Clear) {
  ADAPTER_CALL(SetSelectedIndex(2));
  ADAPTER_CALL(Clear());
  CheckState("active=<none> anchor=<none> selection=");
}

TEST_F(TabStripModelSelectionAdapterTest, AddSelectionFromAnchorTo) {
  ADAPTER_CALL(SetSelectedIndex(2));
  ADAPTER_CALL(AddSelectionFromAnchorTo(4));
  CheckState("active=4 anchor=2 selection=2 3 4");

  ADAPTER_CALL(AddSelectionFromAnchorTo(0));
  CheckState("active=0 anchor=2 selection=0 1 2 3 4");
}

TEST_F(TabStripModelSelectionAdapterTest, Anchor) {
  ADAPTER_CALL(set_anchor(5));
  EXPECT_EQ(5, selection_state_adapter_.anchor());
  EXPECT_EQ(5, list_selection_model_adapter_.anchor());

  ADAPTER_CALL(set_anchor(std::nullopt));
  EXPECT_FALSE(selection_state_adapter_.anchor().has_value());
  EXPECT_FALSE(list_selection_model_adapter_.anchor().has_value());
}

TEST_F(TabStripModelSelectionAdapterTest, Active) {
  ADAPTER_CALL(set_active(5));
  EXPECT_EQ(5, selection_state_adapter_.active());
  EXPECT_EQ(5, list_selection_model_adapter_.active());

  ADAPTER_CALL(set_active(std::nullopt));
  EXPECT_FALSE(selection_state_adapter_.active().has_value());
  EXPECT_FALSE(list_selection_model_adapter_.active().has_value());
}

TEST_F(TabStripModelSelectionAdapterTest, Size) {
  EXPECT_EQ(0u, selection_state_adapter_.size());
  EXPECT_EQ(0u, list_selection_model_adapter_.size());

  ADAPTER_CALL(AddIndexToSelection(2));
  EXPECT_EQ(1u, selection_state_adapter_.size());
  EXPECT_EQ(1u, list_selection_model_adapter_.size());

  ADAPTER_CALL(AddIndexToSelection(4));
  EXPECT_EQ(2u, selection_state_adapter_.size());
  EXPECT_EQ(2u, list_selection_model_adapter_.size());

  ADAPTER_CALL(Clear());
  EXPECT_EQ(0u, selection_state_adapter_.size());
  EXPECT_EQ(0u, list_selection_model_adapter_.size());
}

// Will be enabled after fixing TabStripModel impl in a following CL.
TEST_F(TabStripModelSelectionAdapterTest, DISABLED_IncrementFrom) {
  ADAPTER_CALL(AddIndexRangeToSelection(1, 3));
  ADAPTER_CALL(set_active(2));
  ADAPTER_CALL(set_anchor(1));
  CheckState("active=2 anchor=1 selection=1 2 3");
  ADAPTER_CALL(IncrementFrom(2));
  CheckState("active=3 anchor=1 selection=1 3 4");
}

TEST_F(TabStripModelSelectionAdapterTest, DecrementFrom) {
  ADAPTER_CALL(AddIndexRangeToSelection(1, 3));  // selects 1, 2, 3
  ADAPTER_CALL(set_active(1));
  ADAPTER_CALL(set_anchor(1));
  CheckState("active=1 anchor=1 selection=1 2 3");

  ADAPTER_CALL(DecrementFrom(2, model_.GetTabAtIndex(2)));

  // In these tests theres no expectation that the adapter is hooked up to the
  // tabstripmodel, however because we derive the indexes from the model we must
  // also perform the mutations for insertion and deletion. The adapter call
  // is still correct to happen outside of the CloseWebContentsAt function in
  // the test because the tabstripmodel isnt aware of the test adapter.
  model_.CloseWebContentsAt(2, TabCloseTypes::CLOSE_USER_GESTURE);

  // Active tab is 1, which is before the closed tab. The index should not
  // change. Selection should be 1 and 2 (old 3).
  CheckState("active=1 anchor=1 selection=1 2");
}

TEST_F(TabStripModelSelectionAdapterTest, DecrementFrom_ActiveTab) {
  ADAPTER_CALL(AddIndexRangeToSelection(1, 3));  // selects 1, 2, 3
  ADAPTER_CALL(set_active(2));
  ADAPTER_CALL(set_anchor(1));

  ADAPTER_CALL(DecrementFrom(2, model_.GetTabAtIndex(2)));

  // In these tests theres no expectation that the adapter is hooked up to the
  // tabstripmodel, however because we derive the indexes from the model we must
  // also perform the mutations for insertion and deletion. The adapter call
  // is still correct to happen outside of the CloseWebContentsAt function in
  // the test because the tabstripmodel isnt aware of the test adapter.
  model_.CloseWebContentsAt(2, TabCloseTypes::CLOSE_USER_GESTURE);

  CheckState("active=<none> anchor=1 selection=1 2");
}

TEST_F(TabStripModelSelectionAdapterTest, Move) {
  ADAPTER_CALL(AddIndexToSelection(1));
  ADAPTER_CALL(AddIndexToSelection(4));
  ADAPTER_CALL(set_active(1));
  ADAPTER_CALL(set_anchor(4));

  // this function really only works on the old selection model since it's
  // modifications are completely index based.
  ADAPTER_CALL(Move(1, 3, 1));

  // In these tests theres no expectation that the adapter is hooked up to the
  // tabstripmodel, however because we derive the indexes from the model we must
  // also perform the mutations for insertion and deletion. The adapter call
  // is still correct to happen outside of the CloseWebContentsAt function in
  // the test because the tabstripmodel isnt aware of the test adapter.
  model_.MoveWebContentsAt(1, 3, false);

  CheckState("active=3 anchor=4 selection=3 4");
}

TEST_F(TabStripModelSelectionAdapterTest, Move_NoAnchorActive) {
  ADAPTER_CALL(AddIndexToSelection(1));
  ADAPTER_CALL(AddIndexToSelection(4));
  ADAPTER_CALL(set_active(std::nullopt));
  ADAPTER_CALL(set_anchor(std::nullopt));

  // this function really only works on the old selection model since it's
  // modifications are completely index based.
  ADAPTER_CALL(Move(1, 3, 1));

  // also need to call the tabstrip action here, since thats what performs the
  // move for the new selection state.
  model_.MoveWebContentsAt(1, 3, false);

  CheckState("active=<none> anchor=<none> selection=3 4");
}
