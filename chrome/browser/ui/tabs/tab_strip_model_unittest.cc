// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/test_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

using content::WebContents;

namespace {

const char kTabStripModelTestIDUserDataKey[] = "TabStripModelTestIDUserData";

class TabStripModelTestIDUserData : public base::SupportsUserData::Data {
 public:
  explicit TabStripModelTestIDUserData(int id) : id_(id) {}
  ~TabStripModelTestIDUserData() override {}
  int id() { return id_; }

 private:
  int id_;
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() = default;
  MockTabStripModelObserver(const MockTabStripModelObserver&) = delete;
  MockTabStripModelObserver& operator=(const MockTabStripModelObserver&) =
      delete;
  ~MockTabStripModelObserver() override = default;

  enum TabStripModelObserverAction {
    INSERT,
    CLOSE,
    DETACH,
    ACTIVATE,
    DEACTIVATE,
    SELECT,
    MOVE,
    CHANGE,
    PINNED,
    REPLACED,
    CLOSE_ALL,
    CLOSE_ALL_CANCELED,
    CLOSE_ALL_COMPLETED,
    GROUP_CHANGED,
  };

  struct State {
    State(WebContents* dst_contents,
          std::optional<size_t> dst_index,
          TabStripModelObserverAction action)
        : dst_contents(dst_contents), dst_index(dst_index), action(action) {}

    raw_ptr<WebContents, DanglingUntriaged> src_contents = nullptr;
    raw_ptr<WebContents, DanglingUntriaged> dst_contents;
    std::optional<size_t> src_index;
    std::optional<size_t> dst_index;
    int change_reason = CHANGE_REASON_NONE;
    bool foreground = false;
    TabStripModelObserverAction action;

    std::string ToString() const {
      std::ostringstream oss;
      const auto optional_to_string = [](const auto& opt) {
        return opt.has_value() ? base::NumberToString(opt.value())
                               : std::string("<none>");
      };
      oss << "State change: " << kActionNames[int{action}]
          << "\n  Source index: " << optional_to_string(src_index)
          << "\n  Destination index: " << optional_to_string(dst_index)
          << "\n  Source contents: " << src_contents
          << "\n  Destination contents: " << dst_contents
          << "\n  Change reason: " << change_reason
          << "\n  Foreground: " << (foreground ? "yes" : "no");
      return oss.str();
    }

    bool operator==(const State& state) const {
      return src_contents == state.src_contents &&
             dst_contents == state.dst_contents &&
             src_index == state.src_index && dst_index == state.dst_index &&
             change_reason == state.change_reason &&
             foreground == state.foreground && action == state.action;
    }

   private:
    static const char* const kActionNames[];
  };

  int GetStateCount() const { return static_cast<int>(states_.size()); }

  // Returns (by way of parameters) the number of state's with CLOSE_ALL,
  // CLOSE_ALL_CANCELED and CLOSE_ALL_COMPLETED.
  void GetCloseCounts(int* close_all_count,
                      int* close_all_canceled_count,
                      int* close_all_completed_count) {
    *close_all_count = *close_all_canceled_count = *close_all_completed_count =
        0;
    for (int i = 0; i < GetStateCount(); ++i) {
      switch (GetStateAt(i).action) {
        case CLOSE_ALL:
          (*close_all_count)++;
          break;
        case CLOSE_ALL_CANCELED:
          (*close_all_canceled_count)++;
          break;
        case CLOSE_ALL_COMPLETED:
          (*close_all_completed_count)++;
          break;
        default:
          break;
      }
    }
  }

  const State& GetStateAt(int index) const {
    DCHECK(index >= 0 && index < GetStateCount());
    return states_[index];
  }

  void ExpectStateEquals(int index, const State& state) {
    EXPECT_TRUE(GetStateAt(index) == state)
        << "Got " << GetStateAt(index).ToString() << "\nExpected "
        << state.ToString();
  }

  void PushInsertState(WebContents* contents, int index, bool foreground) {
    State s(contents, index, INSERT);
    s.foreground = foreground;
    states_.push_back(s);
  }

  void PushActivateState(WebContents* old_contents,
                         WebContents* new_contents,
                         std::optional<size_t> index,
                         int reason) {
    State s(new_contents, index, ACTIVATE);
    s.src_contents = old_contents;
    s.change_reason = reason;
    states_.push_back(s);
  }

  void PushDeactivateState(WebContents* contents,
                           const ui::ListSelectionModel& old_model) {
    states_.push_back(State(contents, old_model.active(), DEACTIVATE));
  }

  void PushSelectState(content::WebContents* new_contents,
                       const ui::ListSelectionModel& old_model,
                       const ui::ListSelectionModel& new_model) {
    State s(new_contents, new_model.active(), SELECT);
    s.src_index = old_model.active();
    states_.push_back(s);
  }

  void PushMoveState(WebContents* contents, int from_index, int to_index) {
    const auto tab_index_to_selection_model_index =
        [](int tab_index) -> std::optional<size_t> {
      if (tab_index == TabStripModel::kNoTab)
        return std::nullopt;
      DCHECK_GE(tab_index, 0);
      return static_cast<size_t>(tab_index);
    };
    State s(contents, tab_index_to_selection_model_index(to_index), MOVE);
    s.src_index = tab_index_to_selection_model_index(from_index);
    states_.push_back(s);
  }

  void PushCloseState(WebContents* contents, int index) {
    states_.push_back(State(contents, index, CLOSE));
  }

  void PushDetachState(WebContents* contents, int index, bool was_active) {
    states_.push_back(State(contents, index, DETACH));
  }

  void PushReplaceState(WebContents* old_contents,
                        WebContents* new_contents,
                        int index) {
    State s(new_contents, index, REPLACED);
    s.src_contents = old_contents;
    states_.push_back(s);
  }

  struct TabGroupUpdate {
    int contents_update_count = 0;
    int visuals_update_count = 0;
  };

  const std::map<tab_groups::TabGroupId, TabGroupUpdate>& group_updates()
      const {
    return group_updates_;
  }

  const TabGroupUpdate group_update(tab_groups::TabGroupId group) {
    return group_updates_[group];
  }

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    switch (change.type()) {
      case TabStripModelChange::kInserted: {
        for (const auto& contents : change.GetInsert()->contents) {
          PushInsertState(contents.contents, contents.index,
                          selection.new_contents == contents.contents);
        }
        break;
      }
      case TabStripModelChange::kRemoved: {
        for (const auto& contents : change.GetRemove()->contents) {
          switch (contents.remove_reason) {
            case TabStripModelChange::RemoveReason::kDeleted:
              PushCloseState(contents.contents, contents.index);
              break;
            case TabStripModelChange::RemoveReason::kInsertedIntoOtherTabStrip:
              break;
          }
          PushDetachState(contents.contents, contents.index,
                          selection.old_contents == contents.contents);
        }
        break;
      }
      case TabStripModelChange::kReplaced: {
        auto* replace = change.GetReplace();
        PushReplaceState(replace->old_contents, replace->new_contents,
                         replace->index);
        break;
      }
      case TabStripModelChange::kMoved: {
        auto* move = change.GetMove();
        PushMoveState(move->contents, move->from_index, move->to_index);
        break;
      }
      case TabStripModelChange::kSelectionOnly:
        break;
    }

    if (selection.active_tab_changed()) {
      if (selection.old_contents && selection.selection_changed())
        PushDeactivateState(selection.old_contents, selection.old_model);

      PushActivateState(selection.old_contents, selection.new_contents,
                        selection.new_model.active(), selection.reason);
    }

    if (selection.selection_changed()) {
      PushSelectState(selection.new_contents, selection.old_model,
                      selection.new_model);
    }
  }

  void OnTabGroupChanged(const TabGroupChange& change) override {
    switch (change.type) {
      case TabGroupChange::kCreated: {
        group_updates_[change.group] = TabGroupUpdate();
        break;
      }
      case TabGroupChange::kEditorOpened: {
        break;
      }
      case TabGroupChange::kContentsChanged: {
        group_updates_[change.group].contents_update_count++;
        break;
      }
      case TabGroupChange::kVisualsChanged: {
        group_updates_[change.group].visuals_update_count++;
        break;
      }
      case TabGroupChange::kMoved: {
        break;
      }
      case TabGroupChange::kClosed: {
        group_updates_.erase(change.group);
        break;
      }
    }
  }

  void TabChangedAt(WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    states_.emplace_back(contents, index, CHANGE);
  }

  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             WebContents* contents,
                             int index) override {
    states_.emplace_back(contents, index, PINNED);
  }

  void WillCloseAllTabs(TabStripModel* tab_strip_model) override {
    states_.emplace_back(nullptr, std::nullopt, CLOSE_ALL);
  }

  void CloseAllTabsStopped(TabStripModel* tab_strip_model,
                           CloseAllStoppedReason reason) override {
    if (reason == kCloseAllCanceled) {
      states_.emplace_back(nullptr, std::nullopt, CLOSE_ALL_CANCELED);
    } else if (reason == kCloseAllCompleted) {
      states_.emplace_back(nullptr, std::nullopt, CLOSE_ALL_COMPLETED);
    }
  }

  void ClearStates() { states_.clear(); }

 private:
  std::vector<State> states_;
  std::map<tab_groups::TabGroupId, TabGroupUpdate> group_updates_;
};

const char* const MockTabStripModelObserver::State::kActionNames[]{
    "INSERT",
    "CLOSE",
    "DETACH",
    "ACTIVATE",
    "DEACTIVATE",
    "SELECT",
    "MOVE",
    "CHANGE",
    "PINNED",
    "REPLACED",
    "CLOSE_ALL",
    "CLOSE_ALL_CANCELED",
    "CLOSE_ALL_COMPLETED",
    "GROUP_CHANGED"};

}  // namespace

class TabStripModelTest : public testing::Test,
                          public ::testing::WithParamInterface<bool> {
 public:
  TabStripModelTest() : profile_(new TestingProfile) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/GetParam()
            ? std::vector<
                  base::test::FeatureRef>{tabs::kTabStripCollectionStorage}
            : std::vector<base::test::FeatureRef>{},
        /*disabled_features=*/std::vector<base::test::FeatureRef>{});
  }
  TabStripModelTest(const TabStripModelTest&) = delete;
  TabStripModelTest& operator=(const TabStripModelTest&) = delete;

  TestingProfile* profile() { return profile_.get(); }

  std::unique_ptr<WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

  std::unique_ptr<WebContents> CreateWebContentsWithSharedRPH(
      WebContents* web_contents) {
    WebContents::CreateParams create_params(
        profile(), web_contents->GetPrimaryMainFrame()->GetSiteInstance());
    std::unique_ptr<WebContents> retval = WebContents::Create(create_params);
    EXPECT_EQ(retval->GetPrimaryMainFrame()->GetProcess(),
              web_contents->GetPrimaryMainFrame()->GetProcess());
    return retval;
  }

  std::unique_ptr<WebContents> CreateWebContentsWithID(int id) {
    std::unique_ptr<WebContents> contents = CreateWebContents();
    SetID(contents.get(), id);
    return contents;
  }

  // Sets the id of the specified contents.
  void SetID(WebContents* contents, int id) {
    contents->SetUserData(&kTabStripModelTestIDUserDataKey,
                          std::make_unique<TabStripModelTestIDUserData>(id));
  }

  // Returns the id of the specified contents.
  int GetID(WebContents* contents) {
    TabStripModelTestIDUserData* user_data =
        static_cast<TabStripModelTestIDUserData*>(
            contents->GetUserData(&kTabStripModelTestIDUserDataKey));

    return user_data ? user_data->id() : -1;
  }

  // Returns the state of the given tab strip as a string. The state consists
  // of the ID of each web contents followed by a 'p' if pinned. For example,
  // if the model consists of two tabs with ids 2 and 1, with the first
  // tab pinned, this returns "2p 1".
  std::string GetTabStripStateString(const TabStripModel& model) {
    std::string actual;
    for (int i = 0; i < model.count(); ++i) {
      if (i > 0)
        actual += " ";

      actual += base::NumberToString(GetID(model.GetWebContentsAt(i)));

      if (model.IsTabPinned(i))
        actual += "p";
    }
    return actual;
  }

  void PrepareTabs(TabStripModel* model, int tab_count) {
    for (int i = 0; i < tab_count; ++i)
      model->AppendWebContents(CreateWebContentsWithID(i), true);
  }

  void PrepareTabstripForSelectionTest(TabStripModel* model,
                                       int tab_count,
                                       int pinned_count,
                                       const std::string& selected_tabs) {
    PrepareTabs(model, tab_count);
    for (int i = 0; i < pinned_count; ++i)
      model->SetTabPinned(i, true);

    ui::ListSelectionModel selection_model;
    for (std::string_view sel : base::SplitStringPiece(
             selected_tabs, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
             base::SPLIT_WANT_NONEMPTY)) {
      int value;
      ASSERT_TRUE(base::StringToInt(sel, &value));
      selection_model.AddIndexToSelection(value);
    }
    selection_model.set_active(*selection_model.selected_indices().begin());
    model->SetSelectionFromModel(selection_model);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  const std::unique_ptr<TestingProfile> profile_;
  tabs::PreventTabFeatureInitialization prevent_;
};

TEST_P(TabStripModelTest, TestBasicAPI) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  std::unique_ptr<WebContents> contents1 = CreateWebContentsWithID(1);
  WebContents* raw_contents1 = contents1.get();

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Test AppendWebContents, ContainsIndex
  {
    EXPECT_FALSE(tabstrip.ContainsIndex(0));
    tabstrip.AppendWebContents(std::move(contents1), true);
    EXPECT_TRUE(tabstrip.ContainsIndex(0));
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(3, observer.GetStateCount());
    State s1(raw_contents1, 0, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    observer.ExpectStateEquals(0, s1);
    State s2(raw_contents1, 0, MockTabStripModelObserver::ACTIVATE);
    observer.ExpectStateEquals(1, s2);
    State s3(raw_contents1, 0, MockTabStripModelObserver::SELECT);
    s3.src_index = std::nullopt;
    observer.ExpectStateEquals(2, s3);
    observer.ClearStates();
  }
  EXPECT_EQ("1", GetTabStripStateString(tabstrip));

  // Test InsertWebContentsAt, foreground tab.
  std::unique_ptr<WebContents> contents2 = CreateWebContentsWithID(2);
  content::WebContents* raw_contents2 = contents2.get();
  {
    tabstrip.InsertWebContentsAt(1, std::move(contents2),
                                 AddTabTypes::ADD_ACTIVE);

    EXPECT_EQ(2, tabstrip.count());
    EXPECT_EQ(4, observer.GetStateCount());
    State s1(raw_contents2, 1, MockTabStripModelObserver::INSERT);
    s1.foreground = true;
    observer.ExpectStateEquals(0, s1);
    State s2(raw_contents1, 0, MockTabStripModelObserver::DEACTIVATE);
    observer.ExpectStateEquals(1, s2);
    State s3(raw_contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s3.src_contents = raw_contents1;
    observer.ExpectStateEquals(2, s3);
    State s4(raw_contents2, 1, MockTabStripModelObserver::SELECT);
    s4.src_index = 0;
    observer.ExpectStateEquals(3, s4);
    observer.ClearStates();
  }
  EXPECT_EQ("1 2", GetTabStripStateString(tabstrip));

  // Test InsertWebContentsAt, background tab.
  std::unique_ptr<WebContents> contents3 = CreateWebContentsWithID(3);
  WebContents* raw_contents3 = contents3.get();
  {
    tabstrip.InsertWebContentsAt(2, std::move(contents3),
                                 AddTabTypes::ADD_NONE);

    EXPECT_EQ(3, tabstrip.count());
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(raw_contents3, 2, MockTabStripModelObserver::INSERT);
    s1.foreground = false;
    observer.ExpectStateEquals(0, s1);
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test ActivateTabAt
  {
    tabstrip.ActivateTabAt(
        2, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    EXPECT_EQ(3, observer.GetStateCount());
    State s1(raw_contents2, 1, MockTabStripModelObserver::DEACTIVATE);
    observer.ExpectStateEquals(0, s1);
    State s2(raw_contents3, 2, MockTabStripModelObserver::ACTIVATE);
    s2.src_contents = raw_contents2;
    s2.change_reason = TabStripModelObserver::CHANGE_REASON_USER_GESTURE;
    observer.ExpectStateEquals(1, s2);
    State s3(raw_contents3, 2, MockTabStripModelObserver::SELECT);
    s3.src_index = 1;
    observer.ExpectStateEquals(2, s3);
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test DetachAndDeleteWebContentsAt
  {
    // add a background tab to detach
    std::unique_ptr<WebContents> contents4 = CreateWebContentsWithID(4);
    WebContents* raw_contents4 = contents4.get();
    tabstrip.InsertWebContentsAt(3, std::move(contents4),
                                 AddTabTypes::ADD_NONE);
    // detach and delete the tab
    tabstrip.DetachAndDeleteWebContentsAt(3);

    EXPECT_EQ(3, observer.GetStateCount());
    State s1(raw_contents4, 3, MockTabStripModelObserver::INSERT);
    observer.ExpectStateEquals(0, s1);
    State s2(raw_contents4, 3, MockTabStripModelObserver::CLOSE);
    observer.ExpectStateEquals(1, s2);
    State s3(raw_contents4, 3, MockTabStripModelObserver::DETACH);
    observer.ExpectStateEquals(2, s3);
    observer.ClearStates();
  }
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

  // Test CloseWebContentsAt
  {
    int previous_tab_count = tabstrip.count();
    tabstrip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
    EXPECT_EQ(previous_tab_count - 1, tabstrip.count());

    EXPECT_EQ(5, observer.GetStateCount());
    State s1(raw_contents3, 2, MockTabStripModelObserver::CLOSE);
    observer.ExpectStateEquals(0, s1);
    State s2(raw_contents3, 2, MockTabStripModelObserver::DETACH);
    observer.ExpectStateEquals(1, s2);
    State s3(raw_contents3, 2, MockTabStripModelObserver::DEACTIVATE);
    observer.ExpectStateEquals(2, s3);
    State s4(raw_contents2, 1, MockTabStripModelObserver::ACTIVATE);
    s4.src_contents = raw_contents3;
    s4.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
    observer.ExpectStateEquals(3, s4);
    State s5(raw_contents2, 1, MockTabStripModelObserver::SELECT);
    s5.src_index = 2;
    observer.ExpectStateEquals(4, s5);
    observer.ClearStates();
  }
  EXPECT_EQ("1 2", GetTabStripStateString(tabstrip));

  // Test MoveWebContentsAt, select_after_move == true
  {
    tabstrip.MoveWebContentsAt(1, 0, true);

    EXPECT_EQ(2, observer.GetStateCount());
    State s1(raw_contents2, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    observer.ExpectStateEquals(0, s1);
    EXPECT_EQ(0, tabstrip.active_index());
    observer.ClearStates();
  }
  EXPECT_EQ("2 1", GetTabStripStateString(tabstrip));

  // Test MoveWebContentsAt, select_after_move == false
  {
    tabstrip.MoveWebContentsAt(1, 0, false);
    EXPECT_EQ(2, observer.GetStateCount());
    State s1(raw_contents1, 0, MockTabStripModelObserver::MOVE);
    s1.src_index = 1;
    observer.ExpectStateEquals(0, s1);
    EXPECT_EQ(1, tabstrip.active_index());

    tabstrip.MoveWebContentsAt(0, 1, false);
    observer.ClearStates();
  }
  EXPECT_EQ("2 1", GetTabStripStateString(tabstrip));

  // Test Getters
  {
    EXPECT_EQ(raw_contents2, tabstrip.GetActiveWebContents());
    EXPECT_EQ(raw_contents2, tabstrip.GetWebContentsAt(0));
    EXPECT_EQ(raw_contents1, tabstrip.GetWebContentsAt(1));
    EXPECT_EQ(0, tabstrip.GetIndexOfWebContents(raw_contents2));
    EXPECT_EQ(1, tabstrip.GetIndexOfWebContents(raw_contents1));
  }

  // Test UpdateWebContentsStateAt
  {
    tabstrip.UpdateWebContentsStateAt(0, TabChangeType::kAll);
    EXPECT_EQ(1, observer.GetStateCount());
    State s1(raw_contents2, 0, MockTabStripModelObserver::CHANGE);
    observer.ExpectStateEquals(0, s1);
    observer.ClearStates();
  }

  // Test SelectNextTab, SelectPreviousTab, SelectLastTab
  {
    // Make sure the second of the two tabs is selected first...
    tabstrip.ActivateTabAt(
        1, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    tabstrip.SelectPreviousTab();
    EXPECT_EQ(0, tabstrip.active_index());
    tabstrip.SelectLastTab();
    EXPECT_EQ(1, tabstrip.active_index());
    tabstrip.SelectNextTab();
    EXPECT_EQ(0, tabstrip.active_index());
  }

  // Test CloseSelectedTabs
  {
    tabstrip.CloseSelectedTabs();
    // |CloseSelectedTabs| calls CloseWebContentsAt, we already tested that, now
    // just verify that the count and selected index have changed
    // appropriately...
    EXPECT_EQ(1, tabstrip.count());
    EXPECT_EQ(0, tabstrip.active_index());
  }

  observer.ClearStates();
  tabstrip.CloseAllTabs();

  int close_all_count = 0, close_all_canceled_count = 0,
      close_all_completed_count = 0;
  observer.GetCloseCounts(&close_all_count, &close_all_canceled_count,
                          &close_all_completed_count);
  EXPECT_EQ(1, close_all_count);
  EXPECT_EQ(0, close_all_canceled_count);
  EXPECT_EQ(1, close_all_completed_count);

  // TabStripModel should now be empty.
  EXPECT_TRUE(tabstrip.empty());

  // Opener methods are tested below...

  tabstrip.RemoveObserver(&observer);
}

TEST_P(TabStripModelTest, TestTabHandlesStaticTabstrip) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  tabstrip.AppendWebContents(CreateWebContentsWithID(1), true);
  const tabs::TabHandle handle1 = tabstrip.GetTabHandleAt(0);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), true);
  const tabs::TabHandle handle2 = tabstrip.GetTabHandleAt(1);

  EXPECT_EQ(0, tabstrip.GetIndexOfTab(handle1));
  EXPECT_EQ(handle1, tabstrip.GetTabHandleAt(0));
  EXPECT_EQ(1, tabstrip.GetIndexOfTab(handle2));
  EXPECT_EQ(handle2, tabstrip.GetTabHandleAt(1));
}

TEST_P(TabStripModelTest, TestTabHandlesMovingTabInSameTabstrip) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  tabstrip.AppendWebContents(CreateWebContentsWithID(1), true);
  const tabs::TabHandle handle1 = tabstrip.GetTabHandleAt(0);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), true);
  const tabs::TabHandle handle2 = tabstrip.GetTabHandleAt(1);

  tabstrip.MoveWebContentsAt(0, 1, false);

  EXPECT_EQ(0, tabstrip.GetIndexOfTab(handle2));
  EXPECT_EQ(handle2, tabstrip.GetTabHandleAt(0));
  EXPECT_EQ(1, tabstrip.GetIndexOfTab(handle1));
  EXPECT_EQ(handle1, tabstrip.GetTabHandleAt(1));
}

TEST_P(TabStripModelTest, TestTabHandlesTabClosed) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  tabstrip.AppendWebContents(CreateWebContentsWithID(1), true);
  const tabs::TabHandle handle = tabstrip.GetTabHandleAt(0);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), true);

  tabstrip.CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);

  EXPECT_EQ(TabStripModel::kNoTab, tabstrip.GetIndexOfTab(handle));
  EXPECT_EQ(nullptr, handle.Get());
}

TEST_P(TabStripModelTest, TestTabHandlesOutOfBounds) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  tabstrip.AppendWebContents(CreateWebContentsWithID(1), true);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), true);

  EXPECT_EQ(TabStripModel::kNoTab,
            tabstrip.GetIndexOfTab(tabs::TabHandle::Null()));
  EXPECT_DEATH_IF_SUPPORTED(tabstrip.GetTabHandleAt(2).Get(), "");
  EXPECT_DEATH_IF_SUPPORTED(tabstrip.GetTabHandleAt(-1).Get(), "");
}

TEST_P(TabStripModelTest, TestTabHandlesAcrossModels) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  tabstrip.AppendWebContents(CreateWebContentsWithID(1), true);
  const tabs::TabHandle handle = tabstrip.GetTabHandleAt(0);
  content::WebContents* raw_contents = handle.Get()->contents();
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), true);
  content::WebContents* const opener = tabstrip.GetWebContentsAt(1);

  ASSERT_EQ(0, tabstrip.GetIndexOfTab(handle));
  ASSERT_EQ(handle, tabstrip.GetTabHandleAt(0));
  ASSERT_EQ(&tabstrip, handle.Get()->owning_model());

  tabstrip.SetOpenerOfWebContentsAt(0, opener);
  ASSERT_NE(nullptr, handle.Get()->opener());
  ASSERT_EQ(opener, handle.Get()->opener()->contents());
  tabstrip.SetTabPinned(0, true);
  ASSERT_EQ(true, handle.Get()->pinned());
  tabstrip.SetTabBlocked(0, true);
  ASSERT_EQ(true, handle.Get()->blocked());

  // Detach the tab, and the TabModel should continue to exist, but its state
  // should get mostly reset.

  std::unique_ptr<tabs::TabModel> owned_tab =
      tabstrip.DetachTabAtForInsertion(0);
  EXPECT_EQ(owned_tab.get(), handle.Get());
  EXPECT_EQ(nullptr, handle.Get()->owning_model());

  EXPECT_EQ(raw_contents, handle.Get()->contents());
  EXPECT_EQ(nullptr, handle.Get()->opener());
  EXPECT_EQ(false, handle.Get()->reset_opener_on_active_tab_change());
  EXPECT_EQ(false, handle.Get()->pinned());
  EXPECT_EQ(false, handle.Get()->blocked());

  // Add it back into the tabstrip.

  tabstrip.InsertDetachedTabAt(0, std::move(owned_tab), AddTabTypes::ADD_NONE);
  EXPECT_EQ(&tabstrip, handle.Get()->owning_model());

  EXPECT_EQ(raw_contents, handle.Get()->contents());
  EXPECT_EQ(nullptr, handle.Get()->opener());
  EXPECT_EQ(false, handle.Get()->reset_opener_on_active_tab_change());
  EXPECT_EQ(false, handle.Get()->pinned());
  EXPECT_EQ(false, handle.Get()->blocked());
}

TEST_P(TabStripModelTest, TestBasicOpenerAPI) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // This is a basic test of opener functionality. opener is created
  // as the first tab in the strip and then we create 5 other tabs in the
  // background with opener set as their opener.

  std::unique_ptr<WebContents> opener = CreateWebContents();
  WebContents* raw_opener = opener.get();
  tabstrip.AppendWebContents(std::move(opener), true);
  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  std::unique_ptr<WebContents> contents3 = CreateWebContents();
  std::unique_ptr<WebContents> contents4 = CreateWebContents();
  std::unique_ptr<WebContents> contents5 = CreateWebContents();
  WebContents* raw_contents5 = contents5.get();

  // We use |InsertWebContentsAt| here instead of |AppendWebContents| so that
  // openership relationships are preserved.
  tabstrip.InsertWebContentsAt(tabstrip.count(), std::move(contents1),
                               AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip.InsertWebContentsAt(tabstrip.count(), std::move(contents2),
                               AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip.InsertWebContentsAt(tabstrip.count(), std::move(contents3),
                               AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip.InsertWebContentsAt(tabstrip.count(), std::move(contents4),
                               AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip.InsertWebContentsAt(tabstrip.count(), std::move(contents5),
                               AddTabTypes::ADD_INHERIT_OPENER);

  // All the tabs should have the same opener.
  for (int i = 1; i < tabstrip.count(); ++i) {
    const tabs::TabModel* tab_opener = tabstrip.GetOpenerOfTabAt(i);
    EXPECT_EQ(raw_opener, tab_opener ? tab_opener->contents() : nullptr);
  }

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 1));
  // If the last tab in the opener tree is closed, the preceding tab in the same
  // tree should be selected.
  EXPECT_EQ(4, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 5));

  // Tests the method that finds the last tab opened by the same opener in the
  // strip (this is the insertion index for the next background tab for the
  // specified opener).
  EXPECT_EQ(5, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener, 1));

  // For a tab that has opened no other tabs, the return value should always be
  // -1...
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_contents1, 3));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_contents1, 3));

  // ForgetAllOpeners should destroy all opener relationships.
  tabstrip.ForgetAllOpeners();
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 1));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 5));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener, 1));

  // Specify the last tab as the opener of the others.
  for (int i = 0; i < tabstrip.count() - 1; ++i) {
    tabstrip.SetOpenerOfWebContentsAt(i, raw_contents5);
  }

  for (int i = 0; i < tabstrip.count() - 1; ++i) {
    const tabs::TabModel* tab_opener = tabstrip.GetOpenerOfTabAt(i);
    EXPECT_EQ(raw_contents5, tab_opener ? tab_opener->contents() : nullptr);
  }

  // If there is a next adjacent item, then the index should be of that item.
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_contents5, 1));

  // If the last tab in the opener tree is closed, the preceding tab in the same
  // opener tree should be selected.
  EXPECT_EQ(3, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_contents5, 4));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

static int GetInsertionIndex(TabStripModel* tabstrip) {
  return tabstrip->DetermineInsertionIndex(ui::PAGE_TRANSITION_LINK, false);
}

static void InsertWebContentses(TabStripModel* tabstrip,
                                std::unique_ptr<WebContents> contents1,
                                std::unique_ptr<WebContents> contents2,
                                std::unique_ptr<WebContents> contents3) {
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                std::move(contents1),
                                AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                std::move(contents2),
                                AddTabTypes::ADD_INHERIT_OPENER);
  tabstrip->InsertWebContentsAt(GetInsertionIndex(tabstrip),
                                std::move(contents3),
                                AddTabTypes::ADD_INHERIT_OPENER);
}

static bool IsSiteInContentSettingExceptionList(
    HostContentSettingsMap* settings,
    GURL& url,
    ContentSettingsType type) {
  content_settings::SettingInfo info;
  settings->GetWebsiteSetting(url, url, type, &info);
  auto pattern = ContentSettingsPattern::FromURLNoWildcard(url);
  return info.primary_pattern.Compare(pattern) ==
         ContentSettingsPattern::IDENTITY;
}

// Tests opening background tabs.
TEST_P(TabStripModelTest, TestLTRInsertionOptions) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);

  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  WebContents* raw_contents2 = contents2.get();
  std::unique_ptr<WebContents> contents3 = CreateWebContents();
  WebContents* raw_contents3 = contents3.get();

  // Test LTR
  InsertWebContentses(&tabstrip, std::move(contents1), std::move(contents2),
                      std::move(contents3));
  EXPECT_EQ(raw_contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(raw_contents3, tabstrip.GetWebContentsAt(3));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// This test constructs a tabstrip, and then simulates loading several tabs in
// the background from link clicks on the first tab. Then it simulates opening
// a new tab from the first tab in the foreground via a link click, verifies
// that this tab is opened adjacent to the opener, then closes it.
// Finally it tests that a tab opened for some non-link purpose opens at the
// end of the strip, not bundled to any existing context.
TEST_P(TabStripModelTest, TestInsertionIndexDetermination) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  WebContents* raw_opener = opener.get();
  tabstrip.AppendWebContents(std::move(opener), true);

  // Open some other random unrelated tab in the background to monkey with our
  // insertion index.
  std::unique_ptr<WebContents> other = CreateWebContents();
  WebContents* raw_other = other.get();
  tabstrip.AppendWebContents(std::move(other), false);

  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  WebContents* raw_contents2 = contents2.get();
  std::unique_ptr<WebContents> contents3 = CreateWebContents();
  WebContents* raw_contents3 = contents3.get();

  // Start by testing LTR.
  InsertWebContentses(&tabstrip, std::move(contents1), std::move(contents2),
                      std::move(contents3));
  EXPECT_EQ(raw_opener, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(raw_contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(raw_contents3, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(raw_other, tabstrip.GetWebContentsAt(4));

  // The opener API should work...
  EXPECT_EQ(3, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 2));
  EXPECT_EQ(2, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 3));
  EXPECT_EQ(3, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener, 1));

  // Now open a foreground tab from a link. It should be opened adjacent to the
  // opener tab.
  std::unique_ptr<WebContents> fg_link_contents = CreateWebContents();
  WebContents* raw_fg_link_contents = fg_link_contents.get();
  int insert_index =
      tabstrip.DetermineInsertionIndex(ui::PAGE_TRANSITION_LINK, true);
  EXPECT_EQ(1, insert_index);
  tabstrip.InsertWebContentsAt(
      insert_index, std::move(fg_link_contents),
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  EXPECT_EQ(1, tabstrip.active_index());
  EXPECT_EQ(raw_fg_link_contents, tabstrip.GetActiveWebContents());

  // Now close this contents. The selection should move to the opener contents.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(0, tabstrip.active_index());

  // Now open a new empty tab. It should open at the end of the strip.
  std::unique_ptr<WebContents> fg_nonlink_contents = CreateWebContents();
  WebContents* raw_fg_nonlink_contents = fg_nonlink_contents.get();
  insert_index =
      tabstrip.DetermineInsertionIndex(ui::PAGE_TRANSITION_AUTO_BOOKMARK, true);
  EXPECT_EQ(tabstrip.count(), insert_index);
  // We break the opener relationship...
  tabstrip.InsertWebContentsAt(insert_index, std::move(fg_nonlink_contents),
                               AddTabTypes::ADD_NONE);
  // Now select it, so that GestureType != kNone causes the opener
  // relationship to be forgotten...
  tabstrip.ActivateTabAt(tabstrip.count() - 1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(tabstrip.count() - 1, tabstrip.active_index());
  EXPECT_EQ(raw_fg_nonlink_contents, tabstrip.GetActiveWebContents());

  // Verify that all opener relationships are forgotten.
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 2));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 3));
  EXPECT_EQ(-1, tabstrip.GetIndexOfNextWebContentsOpenedBy(raw_opener, 3));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener, 1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests that non-adjacent tabs with an opener are ignored when deciding where
// to position tabs.
TEST_P(TabStripModelTest, TestInsertionIndexDeterminationAfterDragged) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Start with three tabs, of which the first is active.
  std::unique_ptr<WebContents> opener1 = CreateWebContentsWithID(1);
  WebContents* raw_opener1 = opener1.get();
  tabstrip.AppendWebContents(std::move(opener1), true /* foreground */);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), false);
  tabstrip.AppendWebContents(CreateWebContentsWithID(3), false);
  EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  // Open a link in a new background tab.
  tabstrip.InsertWebContentsAt(GetInsertionIndex(&tabstrip),
                               CreateWebContentsWithID(11),
                               AddTabTypes::ADD_INHERIT_OPENER);
  EXPECT_EQ("1 11 2 3", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  EXPECT_EQ(1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  // Drag that tab (which activates it) one to the right.
  tabstrip.MoveWebContentsAt(1, 2, true /* select_after_move */);
  EXPECT_EQ("1 2 11 3", GetTabStripStateString(tabstrip));
  EXPECT_EQ(11, GetID(tabstrip.GetActiveWebContents()));
  // It should no longer be counted by GetIndexOfLastWebContentsOpenedBy,
  // since there is a tab in between, even though its opener is unchanged.
  // TODO(johnme): Maybe its opener should be reset when it's dragged away.
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  tabs::TabModel* tab_opener = tabstrip.GetOpenerOfTabAt(2);
  EXPECT_EQ(raw_opener1, tab_opener ? tab_opener->contents() : nullptr);

  // Activate the parent tab again.
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));

  // Open another link in a new background tab.
  tabstrip.InsertWebContentsAt(GetInsertionIndex(&tabstrip),
                               CreateWebContentsWithID(12),
                               AddTabTypes::ADD_INHERIT_OPENER);
  // Tab 12 should be next to 1, and considered opened by it.
  EXPECT_EQ("1 12 2 11 3", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  EXPECT_EQ(1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests that grandchild tabs are considered to be opened by their grandparent
// tab when deciding where to position tabs.
TEST_P(TabStripModelTest, TestInsertionIndexDeterminationNestedOpener) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Start with two tabs, of which the first is active:
  std::unique_ptr<WebContents> opener1 = CreateWebContentsWithID(1);
  WebContents* raw_opener1 = opener1.get();
  tabstrip.AppendWebContents(std::move(opener1), true /* foreground */);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), false);
  EXPECT_EQ("1 2", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  EXPECT_EQ(-1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  // Open a link in a new background child tab.
  std::unique_ptr<WebContents> child11 = CreateWebContentsWithID(11);
  WebContents* raw_child11 = child11.get();
  tabstrip.InsertWebContentsAt(GetInsertionIndex(&tabstrip), std::move(child11),
                               AddTabTypes::ADD_INHERIT_OPENER);
  EXPECT_EQ("1 11 2", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  EXPECT_EQ(1, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  // Activate the child tab:
  tabstrip.ActivateTabAt(1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(11, GetID(tabstrip.GetActiveWebContents()));

  // Open a link in a new background grandchild tab.
  tabstrip.InsertWebContentsAt(GetInsertionIndex(&tabstrip),
                               CreateWebContentsWithID(111),
                               AddTabTypes::ADD_INHERIT_OPENER);
  EXPECT_EQ("1 11 111 2", GetTabStripStateString(tabstrip));
  EXPECT_EQ(11, GetID(tabstrip.GetActiveWebContents()));
  // The grandchild tab should be counted by GetIndexOfLastWebContentsOpenedBy
  // as opened by both its parent (child11) and grandparent (opener1).
  EXPECT_EQ(2, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));
  EXPECT_EQ(2, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_child11, 1));

  // Activate the parent tab again:
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));

  // Open another link in a new background child tab (a sibling of child11).
  tabstrip.InsertWebContentsAt(GetInsertionIndex(&tabstrip),
                               CreateWebContentsWithID(12),
                               AddTabTypes::ADD_INHERIT_OPENER);
  EXPECT_EQ("1 11 111 12 2", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  // opener1 has three adjacent descendants (11, 111, 12)
  EXPECT_EQ(3, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));
  // child11 has only one adjacent descendant (111)
  EXPECT_EQ(2, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_child11, 1));

  // Closing a tab should cause its children to inherit the tab's opener.
  int previous_tab_count = tabstrip.count();
  tabstrip.CloseWebContentsAt(1,
                              TabCloseTypes::CLOSE_USER_GESTURE |
                                  TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  EXPECT_EQ(previous_tab_count - 1, tabstrip.count());
  EXPECT_EQ("1 111 12 2", GetTabStripStateString(tabstrip));
  EXPECT_EQ(1, GetID(tabstrip.GetActiveWebContents()));
  // opener1 is now the opener of 111, so has two adjacent descendants (111, 12)
  tabs::TabModel* tab_opener = tabstrip.GetOpenerOfTabAt(1);
  EXPECT_EQ(raw_opener1, tab_opener ? tab_opener->contents() : nullptr);
  EXPECT_EQ(2, tabstrip.GetIndexOfLastWebContentsOpenedBy(raw_opener1, 0));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If the tab is in the background when it is closed, the selection does not
// change.
TEST_P(TabStripModelTest, CloseInactiveTabKeepsSelection) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If the tab doesn't have an opener or a group, selection shifts to the right.
TEST_P(TabStripModelTest, CloseActiveTabShiftsSelectionRight) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tabstrip.ActivateTabAt(1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(1, tabstrip.active_index());

  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If the tab doesn't have an opener but is in a group, selection shifts to
// another tab in the same group.
TEST_P(TabStripModelTest, CloseGroupedTabShiftsSelectionWithinGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());
  ASSERT_TRUE(tabstrip.SupportsTabGroups());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tabstrip.AddToNewGroup({0, 1, 2});
  tabstrip.ActivateTabAt(1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(1, tabstrip.active_index());

  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that the active selection will change to another non collapsed tab when
// the active index is in the collapsing group.
TEST_P(TabStripModelTest, CollapseGroupShiftsSelection_SuccessNextTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());
  ASSERT_TRUE(tabstrip.SupportsTabGroups());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tab_groups::TabGroupId group = tabstrip.AddToNewGroup({0, 1});
  tabstrip.ActivateTabAt(1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(1, tabstrip.active_index());

  std::optional<int> next_active =
      tabstrip.GetNextExpandedActiveTab(tabstrip.active_index(), group);

  EXPECT_EQ(2, next_active);

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that the active selection will change to another non collapsed tab when
// the active index is in the collapsing group.
TEST_P(TabStripModelTest, CollapseGroupShiftsSelection_SuccessPreviousTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());
  ASSERT_TRUE(tabstrip.SupportsTabGroups());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tab_groups::TabGroupId group = tabstrip.AddToNewGroup({2, 3});
  tabstrip.ActivateTabAt(2,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(2, tabstrip.active_index());

  std::optional<int> next_active =
      tabstrip.GetNextExpandedActiveTab(tabstrip.active_index(), group);

  EXPECT_EQ(1, next_active);

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that there is no valid selection to shift to when the active tab is in
// the group that will be collapsed.
TEST_P(TabStripModelTest, CollapseGroupShiftsSelection_NoAvailableTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());
  ASSERT_TRUE(tabstrip.SupportsTabGroups());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ForgetAllOpeners();
  tab_groups::TabGroupId group = tabstrip.AddToNewGroup({0, 1, 2, 3});
  tabstrip.ActivateTabAt(1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(1, tabstrip.active_index());

  std::optional<int> next_active =
      tabstrip.GetNextExpandedActiveTab(tabstrip.active_index(), group);

  EXPECT_EQ(std::nullopt, next_active);

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If the tab does have an opener, selection shifts to the next tab opened by
// the same opener scanning LTR.
TEST_P(TabStripModelTest, CloseTabWithOpenerShiftsSelectionWithinOpened) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  InsertWebContentses(&tabstrip, CreateWebContents(), CreateWebContents(),
                      CreateWebContents());
  ASSERT_EQ(0, tabstrip.active_index());

  tabstrip.ActivateTabAt(2,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(2, tabstrip.active_index());

  tabstrip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(2, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(1, tabstrip.active_index());
  tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that selection is shifted to the correct tab when a tab is closed.
// If the tab has an opener but no "siblings" opened by the same opener,
// selection shifts to the opener itself.
TEST_P(TabStripModelTest, CloseTabWithOpenerShiftsSelectionToOpener) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  tabstrip.AppendWebContents(std::move(opener), true);
  std::unique_ptr<WebContents> other_contents = CreateWebContents();
  tabstrip.InsertWebContentsAt(1, std::move(other_contents),
                               AddTabTypes::ADD_NONE);
  ASSERT_EQ(2, tabstrip.count());
  std::unique_ptr<WebContents> opened_contents = CreateWebContents();
  tabstrip.InsertWebContentsAt(
      2, std::move(opened_contents),
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  ASSERT_EQ(2, tabstrip.active_index());

  tabstrip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTab.
TEST_P(TabStripModelTest, CommandCloseTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Make sure can_close is honored.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 1, 0, "0"));
  EXPECT_TRUE(
      tabstrip.IsContextMenuCommandEnabled(0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  ASSERT_TRUE(tabstrip.empty());

  // Make sure close on a tab that is selected affects all the selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(
      tabstrip.IsContextMenuCommandEnabled(0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tabs 0 and 1.
  EXPECT_EQ("2", GetTabStripStateString(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs and make close on a tab that isn't selected doesn't affect
  // selected tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(
      tabstrip.IsContextMenuCommandEnabled(2, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Tests with 3 tabs, one pinned, two tab selected, one of which is pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "0 1"));
  EXPECT_TRUE(
      tabstrip.IsContextMenuCommandEnabled(0, TabStripModel::CommandCloseTab));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTab);
  // Should have closed tab 2.
  EXPECT_EQ("2", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseOtherTabs.
TEST_P(TabStripModelTest, CommandCloseOtherTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs, select two tabs, CommandCloseOtherTabs should be enabled
  // and close two tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select two tabs, CommandCloseOtherTabs should be enabled and invoking it
  // with a non-selected index should close the two other tabs.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ("0 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Select all, CommandCloseOtherTabs should not be enabled.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1 2"));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandCloseOtherTabs));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // Three tabs, pin one, select the two non-pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "1 2"));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
      1, TabStripModel::CommandCloseOtherTabs));
  // If we don't pass in the pinned index, the command should be enabled.
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());

  // 3 tabs, one pinned.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      1, TabStripModel::CommandCloseOtherTabs));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandCloseOtherTabs));
  tabstrip.ExecuteContextMenuCommand(1, TabStripModel::CommandCloseOtherTabs);
  // The pinned tab shouldn't be closed.
  EXPECT_EQ("0p 1", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandCloseTabsToRight.
TEST_P(TabStripModelTest, CommandCloseTabsToRight) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs, select last two tabs, CommandCloseTabsToRight should
  // only be enabled for the first tab.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "1 2"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
      1, TabStripModel::CommandCloseTabsToRight));
  EXPECT_FALSE(tabstrip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandCloseTabsToRight));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ("0", GetTabStripStateString(tabstrip));
  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandTogglePinned.
TEST_P(TabStripModelTest, CommandTogglePinned) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Create three tabs with one pinned, pin the first two.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 1, "0 1"));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandTogglePinned));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      1, TabStripModel::CommandTogglePinned));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandTogglePinned));
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("0p 1p 2", GetTabStripStateString(tabstrip));

  // Execute CommandTogglePinned again, this should unpin.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("0 1 2", GetTabStripStateString(tabstrip));

  // Pin the last.
  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandTogglePinned);
  EXPECT_EQ("2p 0 1", GetTabStripStateString(tabstrip));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests IsContextMenuCommandEnabled and ExecuteContextMenuCommand with
// CommandToggleGrouped.
TEST_P(TabStripModelTest, CommandToggleGrouped) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());
  ASSERT_TRUE(tabstrip.SupportsTabGroups());

  // Create three tabs, select the first two, and add the first to a group.
  ASSERT_NO_FATAL_FAILURE(
      PrepareTabstripForSelectionTest(&tabstrip, 3, 0, "0 1"));
  tab_groups::TabGroupId original_group = tabstrip.AddToNewGroup({0});
  ASSERT_TRUE(tabstrip.GetTabGroupForTab(0).has_value());

  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleGrouped));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      1, TabStripModel::CommandToggleGrouped));
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandToggleGrouped));

  // Execute CommandToggleGrouped once. Expect both tabs to be in a new group,
  // since they weren't already in the same group.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleGrouped);
  EXPECT_TRUE(tabstrip.GetTabGroupForTab(0).has_value());
  EXPECT_EQ(tabstrip.GetTabGroupForTab(0), tabstrip.GetTabGroupForTab(1));
  EXPECT_NE(tabstrip.GetTabGroupForTab(0), original_group);

  // Only the active tab will remain selected when adding multiple tabs to a
  // group; all selected tabs continue to be selected when removing multiple
  // tabs from a group.
  tabstrip.ToggleSelectionAt(1);

  // Execute CommandToggleGrouped again. Expect both tabs to be ungrouped, since
  // they were in the same group.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleGrouped);
  EXPECT_FALSE(tabstrip.GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(tabstrip.GetTabGroupForTab(1).has_value());

  // Execute CommandToggleGrouped again. Expect both tabs to be grouped again.
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleGrouped);
  EXPECT_TRUE(tabstrip.GetTabGroupForTab(0).has_value());
  EXPECT_EQ(tabstrip.GetTabGroupForTab(0), tabstrip.GetTabGroupForTab(1));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests the following context menu commands:
//  - Close Tab
//  - Close Other Tabs
//  - Close Tabs To Right
TEST_P(TabStripModelTest, TestContextMenuCloseCommands) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  std::unique_ptr<WebContents> opener = CreateWebContents();
  WebContents* raw_opener = opener.get();
  tabstrip.AppendWebContents(std::move(opener), true);

  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  std::unique_ptr<WebContents> contents3 = CreateWebContents();

  InsertWebContentses(&tabstrip, std::move(contents1), std::move(contents2),
                      std::move(contents3));
  EXPECT_EQ(0, tabstrip.active_index());

  tabstrip.ExecuteContextMenuCommand(2, TabStripModel::CommandCloseTab);
  EXPECT_EQ(3, tabstrip.count());

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandCloseTabsToRight);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(raw_opener, tabstrip.GetActiveWebContents());

  std::unique_ptr<WebContents> dummy = CreateWebContents();
  WebContents* raw_dummy = dummy.get();
  tabstrip.AppendWebContents(std::move(dummy), false);

  contents1 = CreateWebContents();
  contents2 = CreateWebContents();
  contents3 = CreateWebContents();
  InsertWebContentses(&tabstrip, std::move(contents1), std::move(contents2),
                      std::move(contents3));
  EXPECT_EQ(5, tabstrip.count());

  int dummy_index = tabstrip.count() - 1;
  tabstrip.ActivateTabAt(dummy_index,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(raw_dummy, tabstrip.GetActiveWebContents());

  tabstrip.ExecuteContextMenuCommand(dummy_index,
                                     TabStripModel::CommandCloseOtherTabs);
  EXPECT_EQ(1, tabstrip.count());
  EXPECT_EQ(raw_dummy, tabstrip.GetActiveWebContents());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

TEST_P(TabStripModelTest, GetIndicesClosedByCommand) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  const auto indicesClosedAsString =
      [&tabstrip](int index, TabStripModel::ContextMenuCommand id) {
        std::vector<int> indices =
            tabstrip.GetIndicesClosedByCommand(index, id);
        std::string result;
        for (size_t i = 0; i < indices.size(); ++i) {
          if (i != 0)
            result += " ";
          result += base::NumberToString(indices[i]);
        }
        return result;
      };

  for (int i = 0; i < 5; ++i)
    tabstrip.AppendWebContents(CreateWebContents(), true);

  EXPECT_EQ("4 3 2 1",
            indicesClosedAsString(0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_EQ("4 3 2",
            indicesClosedAsString(1, TabStripModel::CommandCloseTabsToRight));

  EXPECT_EQ("4 3 2 1",
            indicesClosedAsString(0, TabStripModel::CommandCloseOtherTabs));
  EXPECT_EQ("4 3 2 0",
            indicesClosedAsString(1, TabStripModel::CommandCloseOtherTabs));

  // Pin the first two tabs. Pinned tabs shouldn't be closed by the close other
  // commands.
  tabstrip.SetTabPinned(0, true);
  tabstrip.SetTabPinned(1, true);

  EXPECT_EQ("4 3 2",
            indicesClosedAsString(0, TabStripModel::CommandCloseTabsToRight));
  EXPECT_EQ("4 3",
            indicesClosedAsString(2, TabStripModel::CommandCloseTabsToRight));

  EXPECT_EQ("4 3 2",
            indicesClosedAsString(0, TabStripModel::CommandCloseOtherTabs));
  EXPECT_EQ("4 3",
            indicesClosedAsString(2, TabStripModel::CommandCloseOtherTabs));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not WebContentses are inserted in the correct position
// using this "smart" function with a simulated middle click action on a series
// of links on the home page.
TEST_P(TabStripModelTest, AddWebContents_MiddleClickLinksAndClose) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  std::unique_ptr<WebContents> homepage_contents = CreateWebContents();
  WebContents* raw_homepage_contents = homepage_contents.get();
  tabstrip.AddWebContents(std::move(homepage_contents), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);

  // Open some other tab, by user typing.
  std::unique_ptr<WebContents> typed_page_contents = CreateWebContents();
  WebContents* raw_typed_page_contents = typed_page_contents.get();
  tabstrip.AddWebContents(std::move(typed_page_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  std::unique_ptr<WebContents> middle_click_contents1 = CreateWebContents();
  WebContents* raw_middle_click_contents1 = middle_click_contents1.get();
  tabstrip.AddWebContents(std::move(middle_click_contents1), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> middle_click_contents2 = CreateWebContents();
  WebContents* raw_middle_click_contents2 = middle_click_contents2.get();
  tabstrip.AddWebContents(std::move(middle_click_contents2), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> middle_click_contents3 = CreateWebContents();
  WebContents* raw_middle_click_contents3 = middle_click_contents3.get();
  tabstrip.AddWebContents(std::move(middle_click_contents3), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  EXPECT_EQ(5, tabstrip.count());

  EXPECT_EQ(raw_homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(raw_middle_click_contents1, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_middle_click_contents2, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(raw_middle_click_contents3, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetWebContentsAt(4));

  // Select a tab in the middle of the tabs opened from the home page and start
  // closing them. Each WebContents should be closed, right to left. This test
  // is constructed to start at the middle WebContents in the tree to make sure
  // the cursor wraps around to the first WebContents in the tree before
  // closing the opener or any other WebContents.
  tabstrip.ActivateTabAt(2,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_middle_click_contents3, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_middle_click_contents1, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not a WebContents created by a left click on a link
// that opens a new tab is inserted correctly adjacent to the tab that spawned
// it.
TEST_P(TabStripModelTest, AddWebContents_LeftClickPopup) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  std::unique_ptr<WebContents> homepage_contents = CreateWebContents();
  WebContents* raw_homepage_contents = homepage_contents.get();
  tabstrip.AddWebContents(std::move(homepage_contents), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);

  // Open some other tab, by user typing.
  std::unique_ptr<WebContents> typed_page_contents = CreateWebContents();
  WebContents* raw_typed_page_contents = typed_page_contents.get();
  tabstrip.AddWebContents(std::move(typed_page_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a tab by simulating a left click on a link that opens in a new tab.
  std::unique_ptr<WebContents> left_click_contents = CreateWebContents();
  WebContents* raw_left_click_contents = left_click_contents.get();
  tabstrip.AddWebContents(std::move(left_click_contents), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_ACTIVE);

  // Verify the state meets our expectations.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(raw_left_click_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetWebContentsAt(2));

  // The newly created tab should be selected.
  EXPECT_EQ(raw_left_click_contents, tabstrip.GetActiveWebContents());

  // After closing the selected tab, the selection should move to the left, to
  // the opener.
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(2, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not new tabs that should split context (typed pages,
// generated urls, also blank tabs) open at the end of the tabstrip instead of
// in the middle.
TEST_P(TabStripModelTest, AddWebContents_CreateNewBlankTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  std::unique_ptr<WebContents> homepage_contents = CreateWebContents();
  WebContents* raw_homepage_contents = homepage_contents.get();
  tabstrip.AddWebContents(std::move(homepage_contents), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);

  // Open some other tab, by user typing.
  std::unique_ptr<WebContents> typed_page_contents = CreateWebContents();
  WebContents* raw_typed_page_contents = typed_page_contents.get();
  tabstrip.AddWebContents(std::move(typed_page_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the home page.
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a new blank tab in the foreground.
  std::unique_ptr<WebContents> new_blank_contents = CreateWebContents();
  WebContents* raw_new_blank_contents = new_blank_contents.get();
  tabstrip.AddWebContents(std::move(new_blank_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  // Verify the state of the tabstrip.
  EXPECT_EQ(3, tabstrip.count());
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_new_blank_contents, tabstrip.GetWebContentsAt(2));

  // Now open a couple more blank tabs in the background.
  std::unique_ptr<WebContents> background_blank_contents1 = CreateWebContents();
  WebContents* raw_background_blank_contents1 =
      background_blank_contents1.get();
  tabstrip.AddWebContents(std::move(background_blank_contents1), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> background_blank_contents2 = CreateWebContents();
  WebContents* raw_background_blank_contents2 =
      background_blank_contents2.get();
  tabstrip.AddWebContents(std::move(background_blank_contents2), -1,
                          ui::PAGE_TRANSITION_GENERATED, AddTabTypes::ADD_NONE);
  EXPECT_EQ(5, tabstrip.count());
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetWebContentsAt(0));
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetWebContentsAt(1));
  EXPECT_EQ(raw_new_blank_contents, tabstrip.GetWebContentsAt(2));
  EXPECT_EQ(raw_background_blank_contents1, tabstrip.GetWebContentsAt(3));
  EXPECT_EQ(raw_background_blank_contents2, tabstrip.GetWebContentsAt(4));

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether opener state is correctly forgotten when the user switches
// context.
TEST_P(TabStripModelTest, AddWebContents_ForgetOpeners) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the home page.
  std::unique_ptr<WebContents> homepage_contents = CreateWebContents();
  WebContents* raw_homepage_contents = homepage_contents.get();
  tabstrip.AddWebContents(std::move(homepage_contents), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);

  // Open a blank new tab.
  std::unique_ptr<WebContents> typed_page_contents = CreateWebContents();
  WebContents* raw_typed_page_contents = typed_page_contents.get();
  tabstrip.AddWebContents(std::move(typed_page_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(2, tabstrip.count());

  // Re-select the first tab (home page).
  tabstrip.ActivateTabAt(0,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a bunch of tabs by simulating middle clicking on links on the home
  // page.
  std::unique_ptr<WebContents> middle_click_contents1 = CreateWebContents();
  WebContents* raw_middle_click_contents1 = middle_click_contents1.get();
  tabstrip.AddWebContents(std::move(middle_click_contents1), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> middle_click_contents2 = CreateWebContents();
  WebContents* raw_middle_click_contents2 = middle_click_contents2.get();
  tabstrip.AddWebContents(std::move(middle_click_contents2), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> middle_click_contents3 = CreateWebContents();
  WebContents* raw_middle_click_contents3 = middle_click_contents3.get();
  tabstrip.AddWebContents(std::move(middle_click_contents3), -1,
                          ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  // Break out of the context by selecting a tab in a different context.
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetWebContentsAt(4));
  tabstrip.SelectLastTab();
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetActiveWebContents());

  // Step back into the context by selecting a tab inside it.
  tabstrip.ActivateTabAt(2,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(raw_middle_click_contents2, tabstrip.GetActiveWebContents());

  // Now test that closing tabs selects to the right until there are no more,
  // then to the left, as if there were no context (context has been
  // successfully forgotten).
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_middle_click_contents3, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_typed_page_contents, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_middle_click_contents1, tabstrip.GetActiveWebContents());
  tabstrip.CloseSelectedTabs();
  EXPECT_EQ(raw_homepage_contents, tabstrip.GetActiveWebContents());

  EXPECT_EQ(1, tabstrip.count());

  tabstrip.CloseAllTabs();
  EXPECT_TRUE(tabstrip.empty());
}

// Tests whether or not a WebContents in a new tab belongs in the same tab
// group as its opener.
TEST_P(TabStripModelTest, AddWebContents_LinkOpensInSameGroupAsOpener) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);
  ASSERT_TRUE(tabstrip.empty());

  // Open the home page and add the tab to a group.
  std::unique_ptr<WebContents> homepage_contents = CreateWebContents();
  tabstrip.AddWebContents(std::move(homepage_contents), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);
  ASSERT_EQ(1, tabstrip.count());
  tab_groups::TabGroupId group_id = tabstrip.AddToNewGroup({0});
  ASSERT_EQ(tabstrip.GetTabGroupForTab(0), group_id);
  ASSERT_EQ(observer.group_update(group_id).contents_update_count, 1);

  // Open a tab by simulating a link that opens in a new tab.
  std::unique_ptr<WebContents> contents = CreateWebContents();
  tabstrip.AddWebContents(std::move(contents), -1, ui::PAGE_TRANSITION_LINK,
                          AddTabTypes::ADD_ACTIVE);
  EXPECT_EQ(2, tabstrip.count());
  EXPECT_EQ(tabstrip.GetTabGroupForTab(1), group_id);

  // There should have been a separate notification for the tab being grouped.
  EXPECT_EQ(observer.group_update(group_id).contents_update_count, 2);

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}

// Tests that a inserting a new ungrouped tab between two tabs in the same group
// will add that new tab to the group.
TEST_P(TabStripModelTest, AddWebContents_UngroupedTabDoesNotBreakContinuity) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  ASSERT_TRUE(tabstrip.empty());

  // Open two tabs and add them to a tab group.
  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  tabstrip.AddWebContents(std::move(contents1), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  tabstrip.AddWebContents(std::move(contents2), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);
  ASSERT_EQ(2, tabstrip.count());
  tab_groups::TabGroupId group_id = tabstrip.AddToNewGroup({0, 1});
  ASSERT_EQ(tabstrip.GetTabGroupForTab(0), group_id);
  ASSERT_EQ(tabstrip.GetTabGroupForTab(1), group_id);

  // Open a new tab between the two tabs in a group and ensure the new tab is
  // also in the group.
  std::unique_ptr<WebContents> contents = CreateWebContents();
  WebContents* raw_contents = contents.get();
  tabstrip.AddWebContents(
      std::move(contents), 1, ui::PAGE_TRANSITION_FIRST,
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_FORCE_INDEX);
  EXPECT_EQ(3, tabstrip.count());
  ASSERT_EQ(1, tabstrip.GetIndexOfWebContents(raw_contents));
  EXPECT_EQ(tabstrip.GetTabGroupForTab(1), group_id);

  tabstrip.CloseAllTabs();
  ASSERT_TRUE(tabstrip.empty());
}
// Added for http://b/issue?id=958960
TEST_P(TabStripModelTest, AppendContentsReselectionTest) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  // Open the Home Page.
  tabstrip.AddWebContents(CreateWebContents(), -1,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                          AddTabTypes::ADD_ACTIVE);

  // Open some other tab, by user typing.
  tabstrip.AddWebContents(CreateWebContents(), -1, ui::PAGE_TRANSITION_TYPED,
                          AddTabTypes::ADD_NONE);

  // The selected tab should still be the first.
  EXPECT_EQ(0, tabstrip.active_index());

  // Now simulate a link click that opens a new tab (by virtue of target=_blank)
  // and make sure the correct tab gets selected when the new tab is closed.
  tabstrip.AppendWebContents(CreateWebContents(), true);
  EXPECT_EQ(2, tabstrip.active_index());
  int previous_tab_count = tabstrip.count();
  tabstrip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(previous_tab_count - 1, tabstrip.count());
  EXPECT_EQ(0, tabstrip.active_index());

  // Clean up after ourselves.
  tabstrip.CloseAllTabs();
}

// Added for http://b/issue?id=1027661
TEST_P(TabStripModelTest, ReselectionConsidersChildrenTest) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  std::unique_ptr<WebContents> page_a_contents = CreateWebContents();
  WebContents* raw_page_a_contents = page_a_contents.get();
  strip.AddWebContents(std::move(page_a_contents), -1,
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                       AddTabTypes::ADD_ACTIVE);

  // Simulate middle click to open page A.A and A.B
  std::unique_ptr<WebContents> page_a_a_contents = CreateWebContents();
  WebContents* raw_page_a_a_contents = page_a_a_contents.get();
  strip.AddWebContents(std::move(page_a_a_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  std::unique_ptr<WebContents> page_a_b_contents = CreateWebContents();
  WebContents* raw_page_a_b_contents = page_a_b_contents.get();
  strip.AddWebContents(std::move(page_a_b_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  // Select page A.A
  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(raw_page_a_a_contents, strip.GetActiveWebContents());

  // Simulate a middle click to open page A.A.A
  std::unique_ptr<WebContents> page_a_a_a_contents = CreateWebContents();
  WebContents* raw_page_a_a_a_contents = page_a_a_a_contents.get();
  strip.AddWebContents(std::move(page_a_a_a_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  EXPECT_EQ(raw_page_a_a_a_contents, strip.GetWebContentsAt(2));

  // Close page A.A
  strip.CloseWebContentsAt(strip.active_index(), TabCloseTypes::CLOSE_NONE);

  // Page A.A.A should be selected, NOT A.B
  EXPECT_EQ(raw_page_a_a_a_contents, strip.GetActiveWebContents());

  // Close page A.A.A
  strip.CloseWebContentsAt(strip.active_index(), TabCloseTypes::CLOSE_NONE);

  // Page A.B should be selected
  EXPECT_EQ(raw_page_a_b_contents, strip.GetActiveWebContents());

  // Close page A.B
  strip.CloseWebContentsAt(strip.active_index(), TabCloseTypes::CLOSE_NONE);

  // Page A should be selected
  EXPECT_EQ(raw_page_a_contents, strip.GetActiveWebContents());

  // Clean up.
  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, NewTabAtEndOfStripInheritsOpener) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  strip.AddWebContents(CreateWebContents(), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  for (int i = 0; i < 3; ++i) {
    strip.AddWebContents(CreateWebContents(), -1, ui::PAGE_TRANSITION_LINK,
                         AddTabTypes::ADD_NONE);
  }

  // Switch to page B's tab.
  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a New Tab at the end of the strip (simulate Ctrl+T)
  std::unique_ptr<WebContents> new_contents = CreateWebContents();
  WebContents* raw_new_contents = new_contents.get();
  strip.AddWebContents(std::move(new_contents), -1, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(raw_new_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the New Tab that was just opened. We should be returned to page B's
  // Tab...
  strip.CloseWebContentsAt(4, TabCloseTypes::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with a TYPED transition.
  // This is like typing a URL in the address bar and pressing Alt+Enter. The
  // behavior should be the same as above.
  std::unique_ptr<WebContents> page_e_contents = CreateWebContents();
  WebContents* raw_page_e_contents = page_e_contents.get();
  strip.AddWebContents(std::move(page_e_contents), -1,
                       ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(raw_page_e_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. Selection should shift back to page B's Tab.
  strip.CloseWebContentsAt(4, TabCloseTypes::CLOSE_NONE);

  EXPECT_EQ(1, strip.active_index());

  // Open a non-New Tab tab at the end of the strip, with some other
  // transition. This is like right clicking on a bookmark and choosing "Open
  // in New Tab". No opener relationship should be preserved between this Tab
  // and the one that was active when the gesture was performed.
  std::unique_ptr<WebContents> page_f_contents = CreateWebContents();
  WebContents* raw_page_f_contents = page_f_contents.get();
  strip.AddWebContents(std::move(page_f_contents), -1,
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                       AddTabTypes::ADD_ACTIVE);

  EXPECT_EQ(4, strip.GetIndexOfWebContents(raw_page_f_contents));
  EXPECT_EQ(4, strip.active_index());

  // Close the Tab. The next-adjacent should be selected.
  strip.CloseWebContentsAt(4, TabCloseTypes::CLOSE_NONE);

  EXPECT_EQ(3, strip.active_index());

  // Clean up.
  strip.CloseAllTabs();
}

// A test of navigations in a tab that is part of a tree of openers from some
// parent tab. If the navigations are link clicks, the opener relationships of
// the tab. If they are of any other type, they are not preserved.
TEST_P(TabStripModelTest, NavigationForgetsOpeners) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A
  strip.AddWebContents(CreateWebContents(), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE);

  // Open pages B, C and D in the background from links on page A...
  std::unique_ptr<WebContents> page_c_contents = CreateWebContents();
  WebContents* raw_page_c_contents = page_c_contents.get();
  std::unique_ptr<WebContents> page_d_contents = CreateWebContents();
  WebContents* raw_page_d_contents = page_d_contents.get();
  strip.AddWebContents(CreateWebContents(), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);
  strip.AddWebContents(std::move(page_c_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);
  strip.AddWebContents(std::move(page_d_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);

  // Open page E in a different opener tree from page A.
  std::unique_ptr<WebContents> page_e_contents = CreateWebContents();
  WebContents* raw_page_e_contents = page_e_contents.get();
  strip.AddWebContents(std::move(page_e_contents), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_NONE);

  // Tell the TabStripModel that we are navigating page D via a link click.
  strip.ActivateTabAt(3, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  strip.TabNavigating(raw_page_d_contents, ui::PAGE_TRANSITION_LINK);

  // Close page D, page C should be selected. (part of same opener tree).
  strip.CloseWebContentsAt(3, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(2, strip.active_index());

  // Tell the TabStripModel that we are navigating in page C via a bookmark.
  strip.TabNavigating(raw_page_c_contents, ui::PAGE_TRANSITION_AUTO_BOOKMARK);

  // Close page C, page E should be selected. (C is no longer part of the
  // A-B-C-D tree, selection moves to the right).
  strip.CloseWebContentsAt(2, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(raw_page_e_contents, strip.GetWebContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

// A test for the "quick look" use case where the user can open a new tab at the
// end of the tab strip, do one search, and then close the tab to get back to
// where they were.
TEST_P(TabStripModelTest, NavigationForgettingDoesntAffectNewTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open a tab and several tabs from it, then select one of the tabs that was
  // opened.
  strip.AddWebContents(CreateWebContents(), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE);

  std::unique_ptr<WebContents> page_c_contents = CreateWebContents();
  WebContents* raw_page_c_contents = page_c_contents.get();
  std::unique_ptr<WebContents> page_d_contents = CreateWebContents();
  WebContents* raw_page_d_contents = page_d_contents.get();
  strip.AddWebContents(CreateWebContents(), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);
  strip.AddWebContents(std::move(page_c_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);
  strip.AddWebContents(std::move(page_d_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);

  strip.ActivateTabAt(2, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // TEST 1: A tab in the middle of a bunch of tabs is active and the user opens
  // a new tab at the end of the strip. Closing that new tab will select the tab
  // that they were last on.

  // Open a new tab at the end of the TabStrip.
  std::unique_ptr<WebContents> new_tab_contents = CreateWebContents();
  WebContents* raw_new_tab_contents = new_tab_contents.get();
  content::WebContentsTester::For(raw_new_tab_contents)
      ->NavigateAndCommit(GURL("chrome://newtab"));
  strip.AddWebContents(std::move(new_tab_contents), -1,
                       ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  // The opener should still be remembered after one navigation.
  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://example.com"), raw_new_tab_contents)
      ->Start();
  strip.TabNavigating(raw_new_tab_contents, ui::PAGE_TRANSITION_TYPED);

  // At this point, if we close this tab the last selected one should be
  // re-selected.
  strip.CloseWebContentsAt(strip.count() - 1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(raw_page_c_contents, strip.GetWebContentsAt(strip.active_index()));

  // TEST 2: As above, but the user selects another tab in the strip and thus
  // that new tab's opener relationship is forgotten.

  // Open a new tab again.
  strip.AddWebContents(CreateWebContents(), -1, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_ACTIVE);

  // Now select the first tab.
  strip.ActivateTabAt(0, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Now select the last tab.
  strip.ActivateTabAt(strip.count() - 1,
                      TabStripUserGestureDetails(
                          TabStripUserGestureDetails::GestureType::kOther));

  // Now close the last tab. The next adjacent should be selected.
  strip.CloseWebContentsAt(strip.count() - 1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(raw_page_d_contents, strip.GetWebContentsAt(strip.active_index()));

  // TEST 3: As above, but the user does multiple navigations and thus the tab's
  // opener relationship is forgotten.
  strip.ActivateTabAt(2, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open a new tab but navigate away from the new tab page.
  new_tab_contents = CreateWebContents();
  raw_new_tab_contents = new_tab_contents.get();
  strip.AddWebContents(std::move(new_tab_contents), -1,
                       ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  content::WebContentsTester::For(raw_new_tab_contents)
      ->NavigateAndCommit(GURL("http://example.org"));

  // Do another navigation. The opener should be forgotten.
  content::NavigationSimulator::CreateBrowserInitiated(
      GURL("http://example.com"), raw_new_tab_contents)
      ->Start();
  strip.TabNavigating(raw_new_tab_contents, ui::PAGE_TRANSITION_TYPED);

  // Close the tab. The next adjacent should be selected.
  strip.CloseWebContentsAt(strip.count() - 1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(raw_page_d_contents, strip.GetWebContentsAt(strip.active_index()));

  strip.CloseAllTabs();
}

namespace {

class UnloadListenerTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  UnloadListenerTabStripModelDelegate() = default;
  UnloadListenerTabStripModelDelegate(
      const UnloadListenerTabStripModelDelegate&) = delete;
  UnloadListenerTabStripModelDelegate& operator=(
      const UnloadListenerTabStripModelDelegate&) = delete;
  ~UnloadListenerTabStripModelDelegate() override = default;

  void set_run_unload_listener(bool value) { run_unload_ = value; }

  bool RunUnloadListenerBeforeClosing(WebContents* contents) override {
    return run_unload_;
  }

 private:
  // Whether to report that we need to run an unload listener before closing.
  bool run_unload_ = false;
};

}  // namespace

// Tests that fast shutdown is attempted appropriately.
TEST_P(TabStripModelTest, FastShutdown) {
  UnloadListenerTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  // Make sure fast shutdown is attempted when tabs that share a RPH are shut
  // down.
  {
    std::unique_ptr<WebContents> contents1 = CreateWebContents();
    WebContents* raw_contents1 = contents1.get();
    std::unique_ptr<WebContents> contents2 =
        CreateWebContentsWithSharedRPH(contents1.get());

    SetID(contents1.get(), 1);
    SetID(contents2.get(), 2);

    tabstrip.AppendWebContents(std::move(contents1), true);
    tabstrip.AppendWebContents(std::move(contents2), true);

    // Turn on the fake unload listener so the tabs don't actually get shut
    // down when we call CloseAllTabs()---we need to be able to check that
    // fast shutdown was attempted.
    delegate.set_run_unload_listener(true);
    tabstrip.CloseAllTabs();
    // On a mock RPH this checks whether we *attempted* fast shutdown.
    // A real RPH would reject our attempt since there is an unload handler.
    EXPECT_TRUE(raw_contents1->GetPrimaryMainFrame()
                    ->GetProcess()
                    ->FastShutdownStarted());
    EXPECT_EQ(2, tabstrip.count());

    delegate.set_run_unload_listener(false);
    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }

  // Make sure fast shutdown is not attempted when only some tabs that share a
  // RPH are shut down.
  {
    std::unique_ptr<WebContents> contents1 = CreateWebContents();
    WebContents* raw_contents1 = contents1.get();
    std::unique_ptr<WebContents> contents2 =
        CreateWebContentsWithSharedRPH(contents1.get());

    SetID(contents1.get(), 1);
    SetID(contents2.get(), 2);

    tabstrip.AppendWebContents(std::move(contents1), true);
    tabstrip.AppendWebContents(std::move(contents2), true);

    tabstrip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
    EXPECT_FALSE(raw_contents1->GetPrimaryMainFrame()
                     ->GetProcess()
                     ->FastShutdownStarted());
    EXPECT_EQ(1, tabstrip.count());

    tabstrip.CloseAllTabs();
    EXPECT_TRUE(tabstrip.empty());
  }
}

// Tests various permutations of pinning tabs.
TEST_P(TabStripModelTest, Pinning) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  MockTabStripModelObserver observer;
  tabstrip.AddObserver(&observer);

  EXPECT_TRUE(tabstrip.empty());

  typedef MockTabStripModelObserver::State State;

  std::unique_ptr<WebContents> contents1 = CreateWebContentsWithID(1);
  WebContents* raw_contents1 = contents1.get();
  std::unique_ptr<WebContents> contents3 = CreateWebContentsWithID(3);
  WebContents* raw_contents3 = contents3.get();

  // Note! The ordering of these tests is important, each subsequent test
  // builds on the state established in the previous. This is important if you
  // ever insert tests rather than append.

  // Initial state, three tabs, first selected.
  tabstrip.AppendWebContents(std::move(contents1), true);
  tabstrip.AppendWebContents(CreateWebContentsWithID(2), false);
  tabstrip.AppendWebContents(std::move(contents3), false);

  observer.ClearStates();

  // Pin the first tab, this shouldn't visually reorder anything.
  {
    EXPECT_EQ(0, tabstrip.SetTabPinned(0, true));

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(raw_contents1, 0, MockTabStripModelObserver::PINNED);
    observer.ExpectStateEquals(0, state);

    // And verify the state.
    EXPECT_EQ("1p 2 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Unpin the first tab.
  {
    EXPECT_EQ(0, tabstrip.SetTabPinned(0, false));

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(raw_contents1, 0, MockTabStripModelObserver::PINNED);
    observer.ExpectStateEquals(0, state);

    // And verify the state.
    EXPECT_EQ("1 2 3", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Pin the 3rd tab, which should move it to the front.
  {
    EXPECT_EQ(0, tabstrip.SetTabPinned(2, true));

    // The pinning should have resulted in a move and a pinned notification.
    ASSERT_EQ(3, observer.GetStateCount());
    State state(raw_contents3, 0, MockTabStripModelObserver::MOVE);
    state.src_index = 2;
    observer.ExpectStateEquals(0, state);

    state = State(raw_contents3, 0, MockTabStripModelObserver::PINNED);
    observer.ExpectStateEquals(2, state);

    // And verify the state.
    EXPECT_EQ("3p 1 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Pin the tab "1", which shouldn't move anything.
  {
    EXPECT_EQ(1, tabstrip.SetTabPinned(1, true));

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(1, observer.GetStateCount());
    State state(raw_contents1, 1, MockTabStripModelObserver::PINNED);
    observer.ExpectStateEquals(0, state);

    // And verify the state.
    EXPECT_EQ("3p 1p 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Try to move tab "2" to the front, it should be ignored.
  {
    EXPECT_EQ(2, tabstrip.MoveWebContentsAt(2, 0, false));

    // As the order didn't change, we should get a pinned notification.
    ASSERT_EQ(0, observer.GetStateCount());

    // And verify the state.
    EXPECT_EQ("3p 1p 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Unpin tab "3", which implicitly moves it to the end.
  {
    EXPECT_EQ(1, tabstrip.SetTabPinned(0, false));

    ASSERT_EQ(3, observer.GetStateCount());
    State state(raw_contents3, 1, MockTabStripModelObserver::MOVE);
    state.src_index = 0;
    observer.ExpectStateEquals(0, state);

    state = State(raw_contents3, 1, MockTabStripModelObserver::PINNED);
    observer.ExpectStateEquals(2, state);

    // And verify the state.
    EXPECT_EQ("1p 3 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Unpin tab "3", nothing should happen.
  {
    EXPECT_EQ(1, tabstrip.SetTabPinned(1, false));

    ASSERT_EQ(0, observer.GetStateCount());

    EXPECT_EQ("1p 3 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  // Pin "3" and "1".
  {
    EXPECT_EQ(0, tabstrip.SetTabPinned(0, true));
    EXPECT_EQ(1, tabstrip.SetTabPinned(1, true));

    EXPECT_EQ("1p 3p 2", GetTabStripStateString(tabstrip));

    observer.ClearStates();
  }

  std::unique_ptr<WebContents> contents4 = CreateWebContentsWithID(4);
  WebContents* raw_contents4 = contents4.get();

  // Insert "4" between "1" and "3". As "1" and "4" are pinned, "4" should end
  // up after them.
  {
    EXPECT_EQ(2, tabstrip.InsertWebContentsAt(1, std::move(contents4),
                                              AddTabTypes::ADD_NONE));

    ASSERT_EQ(1, observer.GetStateCount());
    State state(raw_contents4, 2, MockTabStripModelObserver::INSERT);
    observer.ExpectStateEquals(0, state);

    EXPECT_EQ("1p 3p 4 2", GetTabStripStateString(tabstrip));
  }

  tabstrip.CloseAllTabs();
}

// Makes sure the TabStripModel calls the right observer methods during a
// replace.
TEST_P(TabStripModelTest, ReplaceSendsSelected) {
  typedef MockTabStripModelObserver::State State;

  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  std::unique_ptr<WebContents> first_contents = CreateWebContents();
  WebContents* raw_first_contents = first_contents.get();
  strip.AddWebContents(std::move(first_contents), -1, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_ACTIVE);

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);

  std::unique_ptr<WebContents> new_contents = CreateWebContents();
  WebContents* raw_new_contents = new_contents.get();
  strip.DiscardWebContentsAt(0, std::move(new_contents));

  ASSERT_EQ(2, observer.GetStateCount());

  // First event should be for replaced.
  State state(raw_new_contents, 0, MockTabStripModelObserver::REPLACED);
  state.src_contents = raw_first_contents;
  observer.ExpectStateEquals(0, state);

  // And the second for selected.
  state = State(raw_new_contents, 0, MockTabStripModelObserver::ACTIVATE);
  state.src_contents = raw_first_contents;
  state.change_reason = TabStripModelObserver::CHANGE_REASON_REPLACED;
  observer.ExpectStateEquals(1, state);

  // Now add another tab and replace it, making sure we don't get a selected
  // event this time.
  std::unique_ptr<WebContents> third_contents = CreateWebContents();
  WebContents* raw_third_contents = third_contents.get();
  strip.AddWebContents(std::move(third_contents), 1, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_NONE);

  observer.ClearStates();

  // And replace it.
  new_contents = CreateWebContents();
  raw_new_contents = new_contents.get();
  strip.DiscardWebContentsAt(1, std::move(new_contents));

  ASSERT_EQ(1, observer.GetStateCount());

  state = State(raw_new_contents, 1, MockTabStripModelObserver::REPLACED);
  state.src_contents = raw_third_contents;
  observer.ExpectStateEquals(0, state);

  strip.CloseAllTabs();
}

// Ensure pinned tabs are not mixed with non-pinned tabs when using
// MoveWebContentsAt.
TEST_P(TabStripModelTest, MoveWebContentsAtWithPinned) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 6, 3, "0"));
  EXPECT_EQ("0p 1p 2p 3 4 5", GetTabStripStateString(strip));

  // Move middle tabs into the wrong area.
  strip.MoveWebContentsAt(1, 5, true);
  EXPECT_EQ("0p 2p 1p 3 4 5", GetTabStripStateString(strip));
  strip.MoveWebContentsAt(4, 1, true);
  EXPECT_EQ("0p 2p 1p 4 3 5", GetTabStripStateString(strip));

  // Test moving edge cases into the wrong area.
  strip.MoveWebContentsAt(5, 0, true);
  EXPECT_EQ("0p 2p 1p 5 4 3", GetTabStripStateString(strip));
  strip.MoveWebContentsAt(0, 5, true);
  EXPECT_EQ("2p 1p 0p 5 4 3", GetTabStripStateString(strip));

  // Test moving edge cases in the correct area.
  strip.MoveWebContentsAt(3, 5, true);
  EXPECT_EQ("2p 1p 0p 4 3 5", GetTabStripStateString(strip));
  strip.MoveWebContentsAt(2, 0, true);
  EXPECT_EQ("0p 2p 1p 4 3 5", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabNext) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 6, 3, "3"));
  EXPECT_EQ("0p 1p 2p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("0p 1p 2p 4 3 5", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("0p 1p 2p 4 5 3", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("0p 1p 2p 4 5 3", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabNext_Pinned) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 6, 3, "0"));
  EXPECT_EQ("0p 1p 2p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("1p 0p 2p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("1p 2p 0p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabNext();
  EXPECT_EQ("1p 2p 0p 3 4 5", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabNext_Group) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 4, 0, "0"));
  EXPECT_EQ("0 1 2 3", GetTabStripStateString(strip));

  tab_groups::TabGroupId group = strip.AddToNewGroup({1, 2});
  EXPECT_EQ(strip.GetTabGroupForTab(0), std::nullopt);

  strip.MoveTabNext();
  EXPECT_EQ("0 1 2 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);

  strip.MoveTabNext();
  EXPECT_EQ("1 0 2 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);

  strip.MoveTabNext();
  EXPECT_EQ("1 2 0 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(2), group);

  strip.MoveTabNext();
  EXPECT_EQ("1 2 0 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(2), std::nullopt);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabNext_GroupAtEnd) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 2, 0, "0"));
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  tab_groups::TabGroupId group = strip.AddToNewGroup({0, 1});

  strip.MoveTabNext();
  EXPECT_EQ("1 0", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);

  strip.MoveTabNext();
  EXPECT_EQ("1 0", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(1), std::nullopt);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabNext_PinnedDoesNotGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 4, 1, "0"));
  EXPECT_EQ("0p 1 2 3", GetTabStripStateString(strip));

  strip.AddToNewGroup({1, 2});
  EXPECT_EQ(strip.GetTabGroupForTab(0), std::nullopt);

  strip.MoveTabNext();
  EXPECT_EQ("0p 1 2 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(0), std::nullopt);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabPrevious) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 6, 3, "5"));
  EXPECT_EQ("0p 1p 2p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("0p 1p 2p 3 5 4", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("0p 1p 2p 5 3 4", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("0p 1p 2p 5 3 4", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabPrevious_Pinned) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 6, 3, "2"));
  EXPECT_EQ("0p 1p 2p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("0p 2p 1p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("2p 0p 1p 3 4 5", GetTabStripStateString(strip));

  strip.MoveTabPrevious();
  EXPECT_EQ("2p 0p 1p 3 4 5", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabPrevious_Group) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 4, 0, "3"));
  EXPECT_EQ("0 1 2 3", GetTabStripStateString(strip));

  tab_groups::TabGroupId group = strip.AddToNewGroup({1, 2});
  EXPECT_EQ(strip.GetTabGroupForTab(3), std::nullopt);

  strip.MoveTabPrevious();
  EXPECT_EQ("0 1 2 3", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(3), group);

  strip.MoveTabPrevious();
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(2), group);

  strip.MoveTabPrevious();
  EXPECT_EQ("0 3 1 2", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);

  strip.MoveTabPrevious();
  EXPECT_EQ("0 3 1 2", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(1), std::nullopt);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveTabPrevious_GroupAtEnd) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(&strip, 2, 0, "1"));
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  tab_groups::TabGroupId group = strip.AddToNewGroup({0, 1});

  strip.MoveTabPrevious();
  EXPECT_EQ("1 0", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);

  strip.MoveTabPrevious();
  EXPECT_EQ("1 0", GetTabStripStateString(strip));
  EXPECT_EQ(strip.GetTabGroupForTab(0), std::nullopt);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveSelectedTabsTo) {
  struct TestData {
    // Number of tabs the tab strip should have.
    const int tab_count;

    // Number of pinned tabs.
    const int pinned_count;

    // Index of the tabs to select.
    const std::string selected_tabs;

    // Index to move the tabs to.
    const int target_index;

    // Expected state after the move (space separated list of indices).
    const std::string state_after_move;
  } test_data[] = {
      // 1 selected tab.
      {2, 0, "0", 1, "1 0"},
      {3, 0, "0", 2, "1 2 0"},
      {3, 0, "2", 0, "2 0 1"},
      {3, 0, "2", 1, "0 2 1"},
      {3, 0, "0 1", 0, "0 1 2"},

      // 2 selected tabs.
      {6, 0, "4 5", 1, "0 4 5 1 2 3"},
      {3, 0, "0 1", 1, "2 0 1"},
      {4, 0, "0 2", 1, "1 0 2 3"},
      {6, 0, "0 1", 3, "2 3 4 0 1 5"},

      // 3 selected tabs.
      {6, 0, "0 2 3", 3, "1 4 5 0 2 3"},
      {7, 0, "4 5 6", 1, "0 4 5 6 1 2 3"},
      {7, 0, "1 5 6", 4, "0 2 3 4 1 5 6"},

      // 5 selected tabs.
      {8, 0, "0 2 3 6 7", 3, "1 4 5 0 2 3 6 7"},

      // 7 selected tabs
      {16, 0, "0 1 2 3 4 7 9", 8, "5 6 8 10 11 12 13 14 0 1 2 3 4 7 9 15"},

      // With pinned tabs.
      {6, 2, "2 3", 2, "0p 1p 2 3 4 5"},
      {6, 2, "0 4", 3, "1p 0p 2 3 4 5"},
      {6, 3, "1 2 4", 0, "1p 2p 0p 4 3 5"},
      {8, 3, "1 3 4", 4, "0p 2p 1p 5 6 3 4 7"},

      {7, 4, "2 3 4", 3, "0p 1p 2p 3p 5 4 6"},
  };

  for (size_t i = 0; i < std::size(test_data); ++i) {
    TestTabStripModelDelegate delegate;
    TabStripModel strip(&delegate, profile());
    ASSERT_NO_FATAL_FAILURE(PrepareTabstripForSelectionTest(
        &strip, test_data[i].tab_count, test_data[i].pinned_count,
        test_data[i].selected_tabs));
    strip.MoveSelectedTabsTo(test_data[i].target_index, std::nullopt);
    EXPECT_EQ(test_data[i].state_after_move, GetTabStripStateString(strip))
        << i;
    strip.CloseAllTabs();
  }
}

// Tests that moving a tab forgets all openers referencing it.
TEST_P(TabStripModelTest, MoveSelectedTabsTo_ForgetOpeners) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open page A as a new tab and then A1 in the background from A.
  std::unique_ptr<WebContents> page_a_contents = CreateWebContents();
  WebContents* raw_page_a_contents = page_a_contents.get();
  strip.AddWebContents(std::move(page_a_contents), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE);
  std::unique_ptr<WebContents> page_a1_contents = CreateWebContents();
  WebContents* raw_page_a1_contents = page_a1_contents.get();
  strip.AddWebContents(std::move(page_a1_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  // Likewise, open pages B and B1.
  std::unique_ptr<WebContents> page_b_contents = CreateWebContents();
  WebContents* raw_page_b_contents = page_b_contents.get();
  strip.AddWebContents(std::move(page_b_contents), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE);
  std::unique_ptr<WebContents> page_b1_contents = CreateWebContents();
  WebContents* raw_page_b1_contents = page_b1_contents.get();
  strip.AddWebContents(std::move(page_b1_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);

  EXPECT_EQ(raw_page_a_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(raw_page_a1_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(raw_page_b_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(raw_page_b1_contents, strip.GetWebContentsAt(3));

  // Move page B to the start of the tab strip.
  strip.MoveSelectedTabsTo(0, std::nullopt);

  // Open page B2 in the background from B. It should end up after B.
  std::unique_ptr<WebContents> page_b2_contents = CreateWebContents();
  WebContents* raw_page_b2_contents = page_b2_contents.get();
  strip.AddWebContents(std::move(page_b2_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  EXPECT_EQ(raw_page_b_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(raw_page_b2_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(raw_page_a_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(raw_page_a1_contents, strip.GetWebContentsAt(3));
  EXPECT_EQ(raw_page_b1_contents, strip.GetWebContentsAt(4));

  // Switch to A.
  strip.ActivateTabAt(2, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(raw_page_a_contents, strip.GetActiveWebContents());

  // Open page A2 in the background from A. It should end up after A1.
  std::unique_ptr<WebContents> page_a2_contents = CreateWebContents();
  WebContents* raw_page_a2_contents = page_a2_contents.get();
  strip.AddWebContents(std::move(page_a2_contents), -1,
                       ui::PAGE_TRANSITION_LINK, AddTabTypes::ADD_NONE);
  EXPECT_EQ(raw_page_b_contents, strip.GetWebContentsAt(0));
  EXPECT_EQ(raw_page_b2_contents, strip.GetWebContentsAt(1));
  EXPECT_EQ(raw_page_a_contents, strip.GetWebContentsAt(2));
  EXPECT_EQ(raw_page_a1_contents, strip.GetWebContentsAt(3));
  EXPECT_EQ(raw_page_a2_contents, strip.GetWebContentsAt(4));
  EXPECT_EQ(raw_page_b1_contents, strip.GetWebContentsAt(5));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, CloseSelectedTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  for (int i = 0; i < 3; ++i)
    strip.AppendWebContents(CreateWebContents(), true);
  strip.ToggleSelectionAt(1);
  strip.CloseSelectedTabs();
  EXPECT_EQ(1, strip.count());
  EXPECT_EQ(0, strip.active_index());
  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, FirstTabIsActive) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Add the first tab as a background tab.
  strip.AppendWebContents(CreateWebContents(), /*foreground=*/false);

  // The tab is still added as active, because it is the first tab.
  EXPECT_EQ(0, strip.active_index());
}

TEST_P(TabStripModelTest, MultipleSelection) {
  typedef MockTabStripModelObserver::State State;

  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  std::unique_ptr<WebContents> contents0 = CreateWebContents();
  WebContents* raw_contents0 = contents0.get();
  std::unique_ptr<WebContents> contents3 = CreateWebContents();
  WebContents* raw_contents3 = contents3.get();
  strip.AppendWebContents(std::move(contents0), false);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(std::move(contents3), false);
  strip.AddObserver(&observer);

  // Selection and active tab change.
  strip.ActivateTabAt(3, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::ACTIVATE);
  State s1(raw_contents3, 3, MockTabStripModelObserver::SELECT);
  s1.src_index = 0;
  observer.ExpectStateEquals(2, s1);
  observer.ClearStates();

  // Adding all tabs to selection, active tab is now at 0.
  strip.ExtendSelectionTo(0);
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::ACTIVATE);
  State s2(raw_contents0, 0, MockTabStripModelObserver::SELECT);
  s2.src_index = 3;
  observer.ExpectStateEquals(2, s2);
  observer.ClearStates();

  // Toggle the active tab, should make the next index active.
  strip.ToggleSelectionAt(0);
  EXPECT_EQ(1, strip.active_index());
  EXPECT_EQ(3U, strip.selection_model().size());
  EXPECT_EQ(4, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(2).action, MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Toggle the first tab back to selected and active.
  strip.ToggleSelectionAt(0);
  EXPECT_EQ(0, strip.active_index());
  EXPECT_EQ(4U, strip.selection_model().size());
  EXPECT_EQ(4, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(2).action, MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Closing one of the selected tabs, not the active one.
  strip.CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(3, strip.count());
  ASSERT_EQ(3, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action, MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2).action, MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Closing the active tab, while there are others tabs selected.
  strip.CloseWebContentsAt(0, TabCloseTypes::CLOSE_NONE);
  EXPECT_EQ(2, strip.count());
  ASSERT_EQ(5, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action, MockTabStripModelObserver::CLOSE);
  ASSERT_EQ(observer.GetStateAt(1).action, MockTabStripModelObserver::DETACH);
  ASSERT_EQ(observer.GetStateAt(2).action,
            MockTabStripModelObserver::DEACTIVATE);
  ASSERT_EQ(observer.GetStateAt(3).action, MockTabStripModelObserver::ACTIVATE);
  ASSERT_EQ(observer.GetStateAt(4).action, MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Active tab is at 0, deselecting all but the active tab.
  strip.ToggleSelectionAt(1);
  ASSERT_EQ(1, observer.GetStateCount());
  ASSERT_EQ(observer.GetStateAt(0).action, MockTabStripModelObserver::SELECT);
  observer.ClearStates();

  // Attempting to deselect the only selected and therefore active tab,
  // it is ignored (no notifications being sent) and tab at 0 remains selected
  // and active.
  strip.ToggleSelectionAt(0);
  ASSERT_EQ(0, observer.GetStateCount());

  strip.RemoveObserver(&observer);
  strip.CloseAllTabs();
}

// Verifies that if we change the selection from a multi selection to a single
// selection, but not in a way that changes the selected_index that
// TabSelectionChanged is invoked.
TEST_P(TabStripModelTest, MultipleToSingle) {
  typedef MockTabStripModelObserver::State State;

  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  WebContents* raw_contents2 = contents2.get();
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(std::move(contents2), false);
  strip.ToggleSelectionAt(0);
  strip.ToggleSelectionAt(1);

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  // This changes the selection (0 is no longer selected) but the selected_index
  // still remains at 1.
  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(1, observer.GetStateCount());
  State s(raw_contents2, 1, MockTabStripModelObserver::SELECT);
  s.src_index = 1;
  s.change_reason = TabStripModelObserver::CHANGE_REASON_NONE;
  observer.ExpectStateEquals(0, s);
  strip.RemoveObserver(&observer);
  strip.CloseAllTabs();
}

namespace {

// Test Browser-like class for TabStripModelTest.TabBlockedState.
class TabBlockedStateTestBrowser
    : public TabStripModelObserver,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  explicit TabBlockedStateTestBrowser(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    tab_strip_model_->AddObserver(this);
  }
  TabBlockedStateTestBrowser(const TabBlockedStateTestBrowser&) = delete;
  TabBlockedStateTestBrowser& operator=(const TabBlockedStateTestBrowser&) =
      delete;
  ~TabBlockedStateTestBrowser() override {
    tab_strip_model_->RemoveObserver(this);
  }

 private:
  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted)
      return;

    for (const auto& contents : change.GetInsert()->contents) {
      web_modal::WebContentsModalDialogManager* manager =
          web_modal::WebContentsModalDialogManager::FromWebContents(
              contents.contents);
      if (manager)
        manager->SetDelegate(this);
    }
  }

  // WebContentsModalDialogManagerDelegate
  void SetWebContentsBlocked(content::WebContents* contents,
                             bool blocked) override {
    int index = tab_strip_model_->GetIndexOfWebContents(contents);

    // Removal of tabs from the TabStripModel can cause observer callbacks to
    // invoke this method. The WebContents may no longer exist in the
    // TabStripModel.
    if (index == TabStripModel::kNoTab)
      return;

    tab_strip_model_->SetTabBlocked(index, blocked);
  }

  raw_ptr<TabStripModel> tab_strip_model_;
};

class DummySingleWebContentsDialogManager
    : public web_modal::SingleWebContentsDialogManager {
 public:
  explicit DummySingleWebContentsDialogManager(
      gfx::NativeWindow dialog,
      web_modal::SingleWebContentsDialogManagerDelegate* delegate)
      : delegate_(delegate), dialog_(dialog) {}
  DummySingleWebContentsDialogManager(
      const DummySingleWebContentsDialogManager&) = delete;
  DummySingleWebContentsDialogManager& operator=(
      const DummySingleWebContentsDialogManager&) = delete;
  ~DummySingleWebContentsDialogManager() override = default;

  void Show() override {}
  void Hide() override {}
  void Close() override { delegate_->WillClose(dialog_); }
  void Focus() override {}
  void Pulse() override {}
  void HostChanged(web_modal::WebContentsModalDialogHost* new_host) override {}
  gfx::NativeWindow dialog() override { return dialog_; }
  bool IsActive() const override { return true; }

 private:
  raw_ptr<web_modal::SingleWebContentsDialogManagerDelegate> delegate_;
  gfx::NativeWindow dialog_;
};

}  // namespace

// Verifies a newly inserted tab retains its previous blocked state.
// http://crbug.com/276334
TEST_P(TabStripModelTest, TabBlockedState) {
  // Start with a source tab strip.
  TestTabStripModelDelegate dummy_tab_strip_delegate;
  TabStripModel strip_src(&dummy_tab_strip_delegate, profile());
  TabBlockedStateTestBrowser browser_src(&strip_src);

  // Add a tab.
  std::unique_ptr<WebContents> contents1 = CreateWebContents();
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      contents1.get());
  strip_src.AppendWebContents(std::move(contents1), /*foreground=*/true);

  // Add another tab.
  std::unique_ptr<WebContents> contents2 = CreateWebContents();
  WebContents* raw_contents2 = contents2.get();
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      contents2.get());
  strip_src.AppendWebContents(std::move(contents2), false);

  // Create a destination tab strip.
  TabStripModel strip_dst(&dummy_tab_strip_delegate, profile());
  TabBlockedStateTestBrowser browser_dst(&strip_dst);

  // Setup a SingleWebContentsDialogManager for tab |contents2|.
  web_modal::WebContentsModalDialogManager* modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(raw_contents2);

  // Show a dialog that blocks tab |contents2|.
  // DummySingleWebContentsDialogManager doesn't care about the
  // dialog window value, so any dummy value works.
  DummySingleWebContentsDialogManager* native_manager =
      new DummySingleWebContentsDialogManager(gfx::NativeWindow(),
                                              modal_dialog_manager);
  modal_dialog_manager->ShowDialogWithManager(
      gfx::NativeWindow(),
      std::unique_ptr<web_modal::SingleWebContentsDialogManager>(
          native_manager));
  EXPECT_TRUE(strip_src.IsTabBlocked(1));

  // Detach the tab.
  std::unique_ptr<tabs::TabModel> moved_tab =
      strip_src.DetachTabAtForInsertion(1);
  EXPECT_EQ(raw_contents2, moved_tab->contents());

  // Attach the tab to the destination tab strip.
  strip_dst.AppendTab(std::move(moved_tab), true);
  EXPECT_TRUE(strip_dst.IsTabBlocked(0));

  strip_dst.CloseAllTabs();
  strip_src.CloseAllTabs();
}

// Verifies ordering of tabs opened via a link from a pinned tab with a
// subsequent pinned tab.
TEST_P(TabStripModelTest, LinkClicksWithPinnedTabOrdering) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  // Open two pages, pinned.
  strip.AddWebContents(CreateWebContents(), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_PINNED);
  strip.AddWebContents(CreateWebContents(), -1,
                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                       AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_PINNED);

  // Activate the first tab (a).
  strip.ActivateTabAt(0, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  // Open two more tabs as link clicks. The first tab, c, should appear after
  // the pinned tabs followed by the second tab (d).
  std::unique_ptr<WebContents> page_c_contents = CreateWebContents();
  WebContents* raw_page_c_contents = page_c_contents.get();
  std::unique_ptr<WebContents> page_d_contents = CreateWebContents();
  WebContents* raw_page_d_contents = page_d_contents.get();
  strip.AddWebContents(std::move(page_c_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);
  strip.AddWebContents(std::move(page_d_contents), -1, ui::PAGE_TRANSITION_LINK,
                       AddTabTypes::ADD_NONE);

  EXPECT_EQ(2, strip.GetIndexOfWebContents(raw_page_c_contents));
  EXPECT_EQ(3, strip.GetIndexOfWebContents(raw_page_d_contents));
  strip.CloseAllTabs();
}

// This test covers a bug in TabStripModel::MoveWebContentsAt(). Specifically
// if |select_after_move| was true it checked if the index
// select_after_move (as an int) was selected rather than |to_position|.
TEST_P(TabStripModelTest, MoveWebContentsAt) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;

  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AddObserver(&observer);

  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(1, strip.active_index());

  strip.MoveWebContentsAt(2, 3, true);
  EXPECT_EQ(3, strip.active_index());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, NewTabsUngrouped) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), false);

  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());

  strip.ActivateTabAt(0, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_TRUE(strip.SupportsTabGroups());

  strip.AppendWebContents(CreateWebContents(), false);

  strip.AddToNewGroup({0});

  EXPECT_TRUE(strip.GetTabGroupForTab(0).has_value());

  strip.ActivateTabAt(0, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupUpdatesObservers) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  ASSERT_TRUE(strip.SupportsTabGroups());

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  strip.AppendWebContents(CreateWebContents(), true);

  observer.ClearStates();
  tab_groups::TabGroupId group = strip.AddToNewGroup({0});
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(1, observer.group_update(group).contents_update_count);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ReplacingTabGroupUpdatesObservers) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  ASSERT_TRUE(tab_strip.SupportsTabGroups());

  MockTabStripModelObserver observer;
  tab_strip.AddObserver(&observer);
  tab_strip.AppendWebContents(CreateWebContents(), true);
  tab_strip.AppendWebContents(CreateWebContents(), true);

  observer.ClearStates();
  tab_groups::TabGroupId first_group = tab_strip.AddToNewGroup({0, 1});
  tab_groups::TabGroupId second_group = tab_strip.AddToNewGroup({0});
  EXPECT_EQ(2u, observer.group_updates().size());
  EXPECT_EQ(3, observer.group_update(first_group).contents_update_count);
  EXPECT_EQ(1, observer.group_update(second_group).contents_update_count);

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupMiddleOfExistingGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  ASSERT_TRUE(tab_strip.SupportsTabGroups());

  PrepareTabs(&tab_strip, 4);
  tab_strip.AddToNewGroup({0, 1, 2, 3});
  std::optional<tab_groups::TabGroupId> first_group =
      tab_strip.GetTabGroupForTab(0);

  tab_strip.AddToNewGroup({1, 2});

  EXPECT_EQ(tab_strip.GetTabGroupForTab(0), first_group);
  EXPECT_EQ(tab_strip.GetTabGroupForTab(1), first_group);
  EXPECT_NE(tab_strip.GetTabGroupForTab(2), first_group);
  EXPECT_EQ(tab_strip.GetTabGroupForTab(2), tab_strip.GetTabGroupForTab(3));
  EXPECT_EQ("0 3 1 2", GetTabStripStateString(tab_strip));

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupMiddleOfExistingGroupTwoGroups) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  PrepareTabs(&tab_strip, 4);
  tab_strip.AddToNewGroup({0, 1, 2});
  tab_strip.AddToNewGroup({3});

  tab_strip.AddToNewGroup({1});
  EXPECT_NE(tab_strip.GetTabGroupForTab(2), tab_strip.GetTabGroupForTab(0));
  EXPECT_EQ("0 2 1 3", GetTabStripStateString(tab_strip));

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupReorders) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  PrepareTabs(&tab_strip, 3);

  tab_strip.AddToNewGroup({0, 2});

  EXPECT_EQ("0 2 1", GetTabStripStateString(tab_strip));
  EXPECT_EQ(tab_strip.GetTabGroupForTab(0), tab_strip.GetTabGroupForTab(1));
  EXPECT_TRUE(tab_strip.GetTabGroupForTab(0).has_value());
  EXPECT_FALSE(tab_strip.GetTabGroupForTab(2).has_value());

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupUnpins) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  PrepareTabs(&tab_strip, 2);

  tab_strip.SetTabPinned(0, true);
  tab_strip.AddToNewGroup({0, 1});

  EXPECT_EQ("0 1", GetTabStripStateString(tab_strip));

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupUnpinsAndReorders) {
  TestTabStripModelDelegate delegate;
  TabStripModel tab_strip(&delegate, profile());
  PrepareTabs(&tab_strip, 3);

  tab_strip.SetTabPinned(0, true);
  tab_strip.SetTabPinned(1, true);
  tab_strip.AddToNewGroup({0});

  EXPECT_EQ("1p 0 2", GetTabStripStateString(tab_strip));

  tab_strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToNewGroupMovesPinnedAndUnpinnedTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);

  strip.SetTabPinned(0, true);
  strip.SetTabPinned(1, true);
  strip.SetTabPinned(2, true);
  strip.AddToNewGroup({0, 1});
  EXPECT_EQ("2p 0 1 3", GetTabStripStateString(strip));

  strip.AddToNewGroup({0, 2});
  EXPECT_EQ("2 1 0 3", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToExistingGroupIdempotent) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  strip.AppendWebContents(CreateWebContents(), true);

  strip.AddToNewGroup({0});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(0);
  strip.AddToExistingGroup({0}, group.value());

  observer.ClearStates();
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);
  EXPECT_EQ(0, observer.GetStateCount());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToExistingGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);

  strip.AddToNewGroup({0});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(0);

  strip.AddToExistingGroup({1}, group.value());

  EXPECT_EQ(strip.GetTabGroupForTab(1), group);
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToExistingGroupUpdatesObservers) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  PrepareTabs(&strip, 2);

  tab_groups::TabGroupId group = strip.AddToNewGroup({0});
  observer.ClearStates();

  strip.AddToExistingGroup({1}, group);
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(2, observer.group_update(group).contents_update_count);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToLeftOfExistingGroupReorders) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);

  strip.AddToNewGroup({2});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(2);

  strip.AddToExistingGroup({0}, group.value());
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);
  EXPECT_EQ(strip.GetTabGroupForTab(2), group);
  EXPECT_EQ("1 0 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToRighOfExistingGroupReorders) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);

  strip.AddToNewGroup({0});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(0);

  strip.AddToExistingGroup({2}, group.value());
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);
  EXPECT_FALSE(strip.GetTabGroupForTab(2).has_value());
  EXPECT_EQ("0 2 1", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToExistingGroupReorders) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);

  strip.AddToNewGroup({1});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(1);

  strip.AddToExistingGroup({0, 3}, group.value());
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);
  EXPECT_EQ(strip.GetTabGroupForTab(2), group);
  EXPECT_FALSE(strip.GetTabGroupForTab(3).has_value());
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddTabToExistingGroupUnpins) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);

  strip.SetTabPinned(0, true);
  strip.AddToNewGroup({1});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(1);

  strip.AddToExistingGroup({0}, group.value());
  EXPECT_FALSE(strip.IsTabPinned(0));
  EXPECT_EQ(strip.GetTabGroupForTab(0), group);
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, PinTabInGroupUngroups) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);

  strip.AddToNewGroup({0, 1});
  EXPECT_EQ(0, strip.SetTabPinned(1, true));

  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(1).has_value());
  EXPECT_EQ("1p 0", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupNoopForUngroupedTab) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  strip.AppendWebContents(CreateWebContents(), true);

  observer.ClearStates();
  strip.RemoveFromGroup({0});
  EXPECT_EQ(0, observer.GetStateCount());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AddToNewGroup({0});

  strip.RemoveFromGroup({0});

  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupUpdatesObservers) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AppendWebContents(CreateWebContents(), false);
  observer.ClearStates();

  tab_groups::TabGroupId group = strip.AddToNewGroup({0});
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(1, observer.group_update(group).contents_update_count);

  strip.RemoveFromGroup({0});
  EXPECT_EQ(0u, observer.group_updates().size());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupMaintainsOrder) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);
  strip.AddToNewGroup({0, 1});

  strip.RemoveFromGroup({0});

  EXPECT_TRUE(strip.GetTabGroupForTab(1).has_value());
  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupDoesntReorderIfNoGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  strip.AddToNewGroup({0});

  strip.RemoveFromGroup({0, 1});
  EXPECT_EQ("0 1 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest,
       RemoveTabFromGroupMaintainsRelativeOrderOfSelectedTabs) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);
  strip.AddToNewGroup({0, 1, 2, 3});

  strip.RemoveFromGroup({0, 2});

  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(1).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(2).has_value());
  EXPECT_FALSE(strip.GetTabGroupForTab(3).has_value());
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupMixtureOfGroups) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  strip.AddToNewGroup({0, 1});
  strip.AddToNewGroup({2, 3});

  strip.RemoveFromGroup({0, 3, 4});

  EXPECT_FALSE(strip.GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(1).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(2).has_value());
  EXPECT_FALSE(strip.GetTabGroupForTab(3).has_value());
  EXPECT_FALSE(strip.GetTabGroupForTab(4).has_value());
  EXPECT_EQ("0 1 2 3 4", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, RemoveTabFromGroupDeletesGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AddToNewGroup({0});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);

  strip.RemoveFromGroup({0});

  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 0U);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddToNewGroupDeletesGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AddToNewGroup({0});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(0);

  strip.AddToNewGroup({0});

  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);
  EXPECT_NE(strip.GetTabGroupForTab(0), group);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveGroupToTest) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);

  const tab_groups::TabGroupId group1 = strip.AddToNewGroup({0, 1, 2});
  strip.MoveGroupTo(group1, 2);

  EXPECT_EQ("3 4 0 1 2", GetTabStripStateString(strip));
  EXPECT_EQ(group1, strip.GetTabGroupForTab(4));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AddToExistingGroupDeletesGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AddToNewGroup({0});
  strip.AddToNewGroup({1});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 2U);
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(1);

  strip.AddToExistingGroup({1}, strip.GetTabGroupForTab(0).value());

  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);
  EXPECT_NE(strip.group_model()->ListTabGroups()[0], group);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, CloseTabDeletesGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AddToNewGroup({0});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);

  strip.CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);

  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 0U);
}

TEST_P(TabStripModelTest, CloseTabNotifiesObserversOfGroupChange) {
  TestTabStripModelDelegate delegate;
  MockTabStripModelObserver observer;
  TabStripModel strip(&delegate, profile());
  strip.AddObserver(&observer);
  strip.AppendWebContents(CreateWebContents(), true);
  observer.ClearStates();

  tab_groups::TabGroupId group = strip.AddToNewGroup({0});
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(1, observer.group_update(group).contents_update_count);

  strip.CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(0u, observer.group_updates().size());
}

TEST_P(TabStripModelTest, InsertWebContentsAtWithGroupNotifiesObservers) {
  TestTabStripModelDelegate delegate;
  MockTabStripModelObserver observer;
  TabStripModel strip(&delegate, profile());
  strip.AddObserver(&observer);

  strip.AppendWebContents(CreateWebContents(), true);
  strip.AppendWebContents(CreateWebContents(), false);
  observer.ClearStates();

  tab_groups::TabGroupId group = strip.AddToNewGroup({0, 1});
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(2, observer.group_update(group).contents_update_count);

  strip.InsertWebContentsAt(1, CreateWebContents(), AddTabTypes::ADD_NONE,
                            group);

  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(3, observer.group_update(group).contents_update_count);

  strip.CloseAllTabs();
}

// When inserting a WebContents, if a group is not specified, the new tab
// should be left ungrouped.
TEST_P(TabStripModelTest, InsertWebContentsAtDoesNotGroupByDefault) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AppendWebContents(CreateWebContents(), false);
  strip.AddToNewGroup({0, 1});

  strip.InsertWebContentsAt(2, CreateWebContents(), AddTabTypes::ADD_NONE);

  // The newly added tab should not be in the group.
  EXPECT_TRUE(strip.GetTabGroupForTab(0).has_value());
  EXPECT_TRUE(strip.GetTabGroupForTab(1).has_value());
  EXPECT_FALSE(strip.GetTabGroupForTab(2).has_value());

  strip.CloseAllTabs();
}

// When inserting a WebContents, if a group is specified, the new tab should be
// added to that group.
TEST_P(TabStripModelTest, InsertWebContentsAtWithGroupGroups) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContentsWithID(0), true);
  strip.AppendWebContents(CreateWebContentsWithID(1), false);
  strip.AddToNewGroup({0, 1});
  std::optional<tab_groups::TabGroupId> group = strip.GetTabGroupForTab(0);

  strip.InsertWebContentsAt(1, CreateWebContentsWithID(2),
                            AddTabTypes::ADD_NONE, group);

  EXPECT_EQ(strip.GetTabGroupForTab(0), group);
  EXPECT_EQ(strip.GetTabGroupForTab(1), group);
  EXPECT_EQ(strip.GetTabGroupForTab(2), group);
  EXPECT_EQ("0 2 1", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, NewTabWithGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  auto group = strip.AddToNewGroup({1});

  strip.AddWebContents(CreateWebContentsWithID(3), 2, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_NONE, group);
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, NewTabWithGroupDeletedCorrectly) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  strip.AppendWebContents(CreateWebContents(), true);
  strip.AddToNewGroup({0});
  strip.InsertWebContentsAt(1, CreateWebContents(), AddTabTypes::ADD_NONE,
                            strip.GetTabGroupForTab(0));

  strip.RemoveFromGroup({1});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 1U);
  strip.RemoveFromGroup({0});
  EXPECT_EQ(strip.group_model()->ListTabGroups().size(), 0U);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, NewTabWithoutIndexInsertsAtEndOfGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  auto group = strip.AddToNewGroup({0, 1});

  strip.AddWebContents(CreateWebContentsWithID(3), -1,
                       ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_NONE, group);
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, DiscontinuousNewTabIndexTooHigh) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  auto group = strip.AddToNewGroup({0, 1});

  strip.AddWebContents(CreateWebContentsWithID(3), 3, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_NONE, group);
  EXPECT_EQ("0 1 3 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, DiscontinuousNewTabIndexTooLow) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  auto group = strip.AddToNewGroup({1, 2});

  strip.AddWebContents(CreateWebContentsWithID(3), 0, ui::PAGE_TRANSITION_TYPED,
                       AddTabTypes::ADD_NONE, group);
  EXPECT_EQ("0 3 1 2", GetTabStripStateString(strip));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, CreateGroupSetsVisualData) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  tab_groups::ColorLabelMap all_colors = tab_groups::GetTabGroupColorLabelMap();
  PrepareTabs(&strip, all_colors.size() + 1);

  // Expect groups to cycle through the available color set.
  int index = 0;
  for (const auto& color_pair : all_colors) {
    const tab_groups::TabGroupId group = strip.AddToNewGroup({index});
    EXPECT_EQ(strip.group_model()->GetTabGroup(group)->visual_data()->color(),
              color_pair.first);
    ++index;
  }

  // Expect the last group to cycle back to the first color.
  const tab_groups::TabGroupId group = strip.AddToNewGroup({index});
  EXPECT_EQ(strip.group_model()->GetTabGroup(group)->visual_data()->color(),
            all_colors.begin()->first);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, SetVisualDataForGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 1);
  const tab_groups::TabGroupId group = strip.AddToNewGroup({0});

  const tab_groups::TabGroupVisualData new_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan);
  strip.group_model()->GetTabGroup(group)->SetVisualData(new_data);
  const tab_groups::TabGroupVisualData* data =
      strip.group_model()->GetTabGroup(group)->visual_data();
  EXPECT_EQ(data->title(), new_data.title());
  EXPECT_EQ(data->color(), new_data.color());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, VisualDataChangeNotifiesObservers) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);
  PrepareTabs(&strip, 1);
  const tab_groups::TabGroupId group = strip.AddToNewGroup({0});

  // Check that we are notified about the placeholder
  // tab_groups::TabGroupVisualData.
  ASSERT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(1, observer.group_update(group).contents_update_count);

  const tab_groups::TabGroupVisualData new_data(
      u"Foo", tab_groups::TabGroupColorId::kBlue);
  strip.group_model()->GetTabGroup(group)->SetVisualData(new_data);

  // Now check that we are notified when we change it, once at creation
  // and once from the explicit update.
  ASSERT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(2, observer.group_update(group).visuals_update_count);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ObserverCanBeDestroyedEarly) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());

  {
    MockTabStripModelObserver observer;
    strip.AddObserver(&observer);
  }
}

TEST_P(TabStripModelTest, MovingTabToStartOfGroupDoesNotChangeGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group = strip.AddToNewGroup({1, 2, 3});

  strip.MoveWebContentsAt(2, 1, false);
  EXPECT_EQ("0 2 1 3 4", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));
  EXPECT_EQ(group, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingTabToMiddleOfGroupDoesNotChangeGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group = strip.AddToNewGroup({1, 2, 3});

  strip.MoveWebContentsAt(1, 2, false);
  EXPECT_EQ("0 2 1 3 4", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));
  EXPECT_EQ(group, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingTabToEndOfGroupDoesNotChangeGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group = strip.AddToNewGroup({1, 2, 3});

  strip.MoveWebContentsAt(2, 3, false);
  EXPECT_EQ("0 1 3 2 4", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));
  EXPECT_EQ(group, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingTabOutsideOfGroupToStartOfTabstripClearsGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group = strip.AddToNewGroup({1, 2, 3});

  strip.MoveWebContentsAt(1, 0, false);
  EXPECT_EQ("1 0 2 3 4", GetTabStripStateString(strip));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(0));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingTabOutsideOfGroupToEndOfTabstripClearsGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group = strip.AddToNewGroup({1, 2, 3});

  strip.MoveWebContentsAt(3, 4, false);
  EXPECT_EQ("0 1 2 4 3", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(4));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingTabBetweenUngroupedTabsClearsGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  strip.AddToNewGroup({0, 1, 2});

  strip.MoveWebContentsAt(1, 3, false);
  EXPECT_EQ("0 2 3 1 4", GetTabStripStateString(strip));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(3));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(4));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingUngroupedTabBetweenGroupsDoesNotAssignGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group1 = strip.AddToNewGroup({1, 2});
  const auto group2 = strip.AddToNewGroup({3, 4});

  strip.MoveWebContentsAt(0, 2, false);
  EXPECT_EQ("1 2 0 3 4", GetTabStripStateString(strip));
  EXPECT_EQ(group1, strip.GetTabGroupForTab(1));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(2));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest,
       MovingUngroupedTabBetweenGroupAndUngroupedDoesNotAssignGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);
  const auto group = strip.AddToNewGroup({1, 2});

  strip.MoveWebContentsAt(0, 2, false);
  EXPECT_EQ("1 2 0 3", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(1));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest,
       MovingUngroupedTabBetweenUngroupedAndGroupDoesNotAssignGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);
  const auto group = strip.AddToNewGroup({2, 3});

  strip.MoveWebContentsAt(0, 1, false);
  EXPECT_EQ("1 0 2 3", GetTabStripStateString(strip));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(0));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest,
       MovingGroupMemberBetweenTwoDifferentGroupsClearsGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 6);
  const auto group1 = strip.AddToNewGroup({0, 1});
  const auto group2 = strip.AddToNewGroup({2, 3});
  const auto group3 = strip.AddToNewGroup({4, 5});

  strip.MoveWebContentsAt(0, 3, false);
  EXPECT_EQ("1 2 3 0 4 5", GetTabStripStateString(strip));
  EXPECT_EQ(group1, strip.GetTabGroupForTab(0));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(2));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(3));
  EXPECT_EQ(group3, strip.GetTabGroupForTab(4));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest,
       MovingSingleTabGroupBetweenTwoGroupsDoesNotClearGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 5);
  const auto group1 = strip.AddToNewGroup({0});
  const auto group2 = strip.AddToNewGroup({1, 2});
  const auto group3 = strip.AddToNewGroup({3, 4});

  strip.MoveWebContentsAt(0, 2, false);
  EXPECT_EQ("1 2 0 3 4", GetTabStripStateString(strip));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(0));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group1, strip.GetTabGroupForTab(2));
  EXPECT_EQ(group3, strip.GetTabGroupForTab(3));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingUngroupedTabIntoGroupSetsGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  const auto group = strip.AddToNewGroup({1, 2});

  strip.MoveWebContentsAt(0, 1, false);
  EXPECT_EQ("1 0 2", GetTabStripStateString(strip));
  EXPECT_EQ(group, strip.GetTabGroupForTab(0));
  EXPECT_EQ(group, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group, strip.GetTabGroupForTab(2));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MovingGroupedTabIntoGroupChangesGroup) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  strip.AddToNewGroup({0});
  const auto group2 = strip.AddToNewGroup({1, 2});

  strip.MoveWebContentsAt(0, 1, false);
  EXPECT_EQ("1 0 2", GetTabStripStateString(strip));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(0));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(1));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(2));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveWebContentsAtCorrectlyRemovesGroupEntries) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);
  strip.AddToNewGroup({0});
  const auto group2 = strip.AddToNewGroup({1, 2});

  strip.MoveWebContentsAt(0, 1, false);
  EXPECT_EQ("1 0 2", GetTabStripStateString(strip));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(1));
  const std::vector<tab_groups::TabGroupId> expected_groups{group2};
  EXPECT_EQ(expected_groups, strip.group_model()->ListTabGroups());

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveWebContentsAtCorrectlySendsGroupChangedEvent) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);

  const tab_groups::TabGroupId group1 = strip.AddToNewGroup({0});
  const tab_groups::TabGroupId group2 = strip.AddToNewGroup({1, 2});

  EXPECT_EQ(2u, observer.group_updates().size());
  EXPECT_EQ(1, observer.group_update(group1).contents_update_count);
  EXPECT_EQ(2, observer.group_update(group2).contents_update_count);

  strip.MoveWebContentsAt(0, 1, false);
  EXPECT_EQ("1 0 2", GetTabStripStateString(strip));
  EXPECT_EQ(group2, strip.GetTabGroupForTab(1));

  // group1 should be deleted. group2 should have an update.
  EXPECT_EQ(1u, observer.group_updates().size());
  EXPECT_EQ(3, observer.group_update(group2).contents_update_count);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, MoveWebContentsAtCorrectlySendsGroupClearedEvent) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);

  MockTabStripModelObserver observer;
  strip.AddObserver(&observer);

  const tab_groups::TabGroupId group1 = strip.AddToNewGroup({0, 1});
  const tab_groups::TabGroupId group2 = strip.AddToNewGroup({2});
  EXPECT_EQ(2u, observer.group_updates().size());
  EXPECT_EQ(2, observer.group_update(group1).contents_update_count);
  EXPECT_EQ(1, observer.group_update(group2).contents_update_count);

  strip.MoveWebContentsAt(0, 2, false);
  EXPECT_EQ("1 2 0", GetTabStripStateString(strip));
  EXPECT_EQ(std::nullopt, strip.GetTabGroupForTab(2));

  // The tab should be removed from group1 but not added to group2.
  EXPECT_EQ(2u, observer.group_updates().size());
  EXPECT_EQ(3, observer.group_update(group1).contents_update_count);
  EXPECT_EQ(1, observer.group_update(group2).contents_update_count);

  strip.CloseAllTabs();
}

// Ensure that the opener for a tab never refers to a dangling WebContents.
// Regression test for crbug.com/1092308.
TEST_P(TabStripModelTest, DanglingOpener) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);

  WebContents* contents_1 = strip.GetWebContentsAt(0);
  WebContents* contents_2 = strip.GetWebContentsAt(1);
  ASSERT_TRUE(contents_1);
  ASSERT_TRUE(contents_2);

  // Set the openers for the two tabs to each other.
  strip.SetOpenerOfWebContentsAt(0, contents_2);
  strip.SetOpenerOfWebContentsAt(1, contents_1);
  EXPECT_EQ("0 1", GetTabStripStateString(strip));

  // Move the first tab to the end of the tab strip.
  EXPECT_EQ(1, strip.MoveWebContentsAt(0, 1, false /* select_after_move */));
  EXPECT_EQ("1 0", GetTabStripStateString(strip));

  // Replace the WebContents at index 0 with a new WebContents.
  std::unique_ptr<WebContents> replaced_contents =
      strip.DiscardWebContentsAt(0, CreateWebContentsWithID(5));
  EXPECT_EQ(contents_2, replaced_contents.get());
  replaced_contents.reset();
  EXPECT_EQ("5 0", GetTabStripStateString(strip));
  ASSERT_TRUE(contents_2);

  // Ensure the opener for the tab at index 0 isn't dangling. It should be null
  // instead.
  tabs::TabModel* opener = strip.GetOpenerOfTabAt(0);
  EXPECT_FALSE(opener);

  strip.CloseAllTabs();
}

class TabToWindowTestTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  bool CanMoveTabsToWindow(const std::vector<int>& indices) override {
    for (int index : indices)
      can_move_calls_.push_back(index);
    return true;
  }

  void MoveTabsToNewWindow(const std::vector<int>& indices) override {
    for (int index : indices)
      move_calls_.push_back(index);
  }

  void MoveGroupToNewWindow(const tab_groups::TabGroupId& group) override {}

  std::vector<int> can_move_calls() { return can_move_calls_; }
  std::vector<int> move_calls() { return move_calls_; }

 private:
  std::vector<int> can_move_calls_;
  std::vector<int> move_calls_;
};

// Sanity check to ensure that the "Move Tabs to Window" command talks to
// the delegate correctly.
TEST_P(TabStripModelTest, MoveTabsToNewWindow) {
  TabToWindowTestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 3);

  EXPECT_EQ(delegate.can_move_calls().size(), 0u);
  EXPECT_EQ(delegate.move_calls().size(), 0u);

  EXPECT_TRUE(strip.IsContextMenuCommandEnabled(
      2, TabStripModel::CommandMoveTabsToNewWindow));

  // ASSERT and not EXPECT since we're accessing back() later.
  ASSERT_EQ(delegate.can_move_calls().size(), 1u);
  EXPECT_EQ(delegate.move_calls().size(), 0u);
  EXPECT_EQ(delegate.can_move_calls().back(), 2);

  strip.ExecuteContextMenuCommand(0, TabStripModel::CommandMoveTabsToNewWindow);
  // Whether ExecuteCommand checks if the command is valid or not is an
  // implementation detail, so let's not be brittle.
  EXPECT_GT(delegate.can_move_calls().size(), 0u);
  ASSERT_EQ(delegate.move_calls().size(), 1u);
  EXPECT_EQ(delegate.move_calls().back(), 0);

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, SurroundingGroupAtIndex) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 4);

  auto group1 = strip.AddToNewGroup({1, 2});
  strip.AddToNewGroup({3});

  EXPECT_EQ(std::nullopt, strip.GetSurroundingTabGroup(0));
  EXPECT_EQ(std::nullopt, strip.GetSurroundingTabGroup(1));
  EXPECT_EQ(group1, strip.GetSurroundingTabGroup(2));
  EXPECT_EQ(std::nullopt, strip.GetSurroundingTabGroup(3));
  EXPECT_EQ(std::nullopt, strip.GetSurroundingTabGroup(4));

  strip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ActivateRecordsStartTime) {
  TestTabStripModelDelegate delegate;
  TabStripModel strip(&delegate, profile());
  PrepareTabs(&strip, 2);

  auto has_tab_switch_start_time = [&strip](int index) -> bool {
    return !content::WebContentsTester::For(strip.GetWebContentsAt(index))
                ->GetTabSwitchStartTime()
                .is_null();
  };

  // PrepareTabs should leave the last tab active.
  ASSERT_EQ(strip.GetActiveWebContents(), strip.GetWebContentsAt(1));
  ASSERT_FALSE(has_tab_switch_start_time(0));
  ASSERT_FALSE(has_tab_switch_start_time(1));

  // ActivateTabAt should only update the start time if the active tab changes.
  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_FALSE(has_tab_switch_start_time(0));
  EXPECT_FALSE(has_tab_switch_start_time(1));
  strip.ActivateTabAt(0, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(has_tab_switch_start_time(0));
  EXPECT_FALSE(has_tab_switch_start_time(1));
  strip.ActivateTabAt(1, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_TRUE(has_tab_switch_start_time(0));
  EXPECT_TRUE(has_tab_switch_start_time(1));
}

TEST_P(TabStripModelTest, ToggleSiteMuted) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  GURL url("https://example.com/");
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile());

  std::unique_ptr<WebContents> new_tab_contents = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents.get())
      ->SetLastCommittedURL(url);

  tabstrip.AddWebContents(std::move(new_tab_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  // Validate if the mute site menu item shows up and the site is unmuted
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleSiteMuted));
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));

  // Validate if toggling the state successfully mutes the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteInContentSettingExceptionList(settings, url,
                                                  ContentSettingsType::SOUND));

  // Toggling the state again to successfully unmute the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_FALSE(IsSiteInContentSettingExceptionList(settings, url,
                                                   ContentSettingsType::SOUND));

  tabstrip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ToggleSiteMutedWithLessSpecificRule) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  GURL url("https://example.com/");
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile());

  std::unique_ptr<WebContents> new_tab_contents = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents.get())
      ->SetLastCommittedURL(url);

  tabstrip.AddWebContents(std::move(new_tab_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  // Validate if the mute site menu item shows up and the site is unmuted
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleSiteMuted));
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));

  // Add a wildcard to mute all HTTPS sites as a custom behavior
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://*");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromString("*");
  settings->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                         ContentSettingsType::SOUND,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));

  // Validate we are able to unmute the site (with the wildcard custom behavior)
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteInContentSettingExceptionList(settings, url,
                                                  ContentSettingsType::SOUND));

  // Validate we are able to mute the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));

  // Validate we are able to unmute the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteInContentSettingExceptionList(settings, url,
                                                  ContentSettingsType::SOUND));

  tabstrip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ToggleSiteMutedWithOtherDisjointRule) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  GURL url("https://example.com/");
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile());

  std::unique_ptr<WebContents> new_tab_contents = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents.get())
      ->SetLastCommittedURL(url);

  tabstrip.AddWebContents(std::move(new_tab_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  // Validate if the mute site menu item shows up and the site is unmuted
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleSiteMuted));
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));

  // Add a wildcard to mute all HTTPS sites as a custom behavior
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://www.google.com");
  ContentSettingsPattern secondary_pattern =
      ContentSettingsPattern::FromString("*");
  settings->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                         ContentSettingsType::SOUND,
                                         CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));

  // Validate we are able to mute the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteInContentSettingExceptionList(settings, url,
                                                  ContentSettingsType::SOUND));

  // Validate we are able to unmute the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_FALSE(IsSiteInContentSettingExceptionList(settings, url,
                                                   ContentSettingsType::SOUND));

  tabstrip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ToggleSiteMutedWithDifferentDefault) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  GURL url("https://example.com/");
  HostContentSettingsMap* settings =
      HostContentSettingsMapFactory::GetForProfile(profile());

  std::unique_ptr<WebContents> new_tab_contents = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents.get())
      ->SetLastCommittedURL(url);

  tabstrip.AddWebContents(std::move(new_tab_contents), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

  settings->SetDefaultContentSetting(ContentSettingsType::SOUND,
                                     ContentSetting::CONTENT_SETTING_BLOCK);

  // Validate if the mute site menu item shows up and the site is muted
  EXPECT_TRUE(tabstrip.IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleSiteMuted));
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));

  // Validate if toggling the state successfully unmutes the site
  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteInContentSettingExceptionList(settings, url,
                                                  ContentSettingsType::SOUND));

  tabstrip.CloseAllTabs();
}

TEST_P(TabStripModelTest, ToggleMuteUnmuteMultipleSites) {
  TestTabStripModelDelegate delegate;
  TabStripModel tabstrip(&delegate, profile());
  EXPECT_TRUE(tabstrip.empty());

  GURL url1("https://example1.com/");
  std::unique_ptr<WebContents> new_tab_contents1 = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents1.get())
      ->SetLastCommittedURL(url1);
  tabstrip.AddWebContents(std::move(new_tab_contents1), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));

  GURL url2("https://example2.com/");
  std::unique_ptr<WebContents> new_tab_contents2 = CreateWebContents();
  content::WebContentsTester::For(new_tab_contents2.get())
      ->SetLastCommittedURL(url2);
  tabstrip.AddWebContents(std::move(new_tab_contents2), -1,
                          ui::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 1));

  EXPECT_TRUE(tabstrip.ToggleSelectionAt(0));
  EXPECT_TRUE(tabstrip.selection_model().IsSelected(1));

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_TRUE(IsSiteMuted(tabstrip, 0));
  EXPECT_TRUE(IsSiteMuted(tabstrip, 1));

  tabstrip.ExecuteContextMenuCommand(0, TabStripModel::CommandToggleSiteMuted);
  EXPECT_FALSE(IsSiteMuted(tabstrip, 0));
  EXPECT_FALSE(IsSiteMuted(tabstrip, 1));

  tabstrip.CloseAllTabs();
}

TEST_P(TabStripModelTest, AppendTab) {
  TestTabStripModelDelegate delegate;
  std::unique_ptr<TabStripModel> tabstrip =
      std::make_unique<TabStripModel>(&delegate, profile());
  ASSERT_TRUE(tabstrip->empty());

  // Create a 2 tabs to serve as an opener and the previous opener.
  PrepareTabs(tabstrip.get(), 4);
  ASSERT_EQ(4, tabstrip->count());

  // Force the opener of tab in index 1 to be tab at index 0.
  tabstrip->SetOpenerOfWebContentsAt(1, tabstrip->GetWebContentsAt(0));
  ASSERT_EQ(tabstrip->GetTabAtIndex(0),
            tabstrip->GetTabHandleAt(1).Get()->opener());

  // Detach 2 tabs for the test, one for each option.
  std::unique_ptr<tabs::TabModel> tab_model_with_foreground_true =
      tabstrip->DetachTabAtForInsertion(2);
  tabs::TabModel* tab_model_with_foreground_true_ptr =
      tab_model_with_foreground_true.get();
  ASSERT_EQ(3, tabstrip->count());
  ASSERT_EQ(tab_model_with_foreground_true->owning_model(), nullptr);

  std::unique_ptr<tabs::TabModel> tab_model_with_foreground_false =
      tabstrip->DetachTabAtForInsertion(2);
  tabs::TabModel* tab_model_with_foreground_false_ptr =
      tab_model_with_foreground_false.get();
  ASSERT_EQ(2, tabstrip->count());
  ASSERT_EQ(tab_model_with_foreground_false->owning_model(), nullptr);

  // Add a 3rd tab using the foreground option. When the foreground option is
  // used, the new tab should become active, and the previous tab should become
  // the opener for the newly active tab.
  tabstrip->AppendTab(std::move(tab_model_with_foreground_true),
                      /*foreground=*/true);
  EXPECT_TRUE(tabstrip->ContainsIndex(2));
  EXPECT_EQ(2, tabstrip->active_index());
  EXPECT_EQ(tabstrip->GetTabAtIndex(1),
            tabstrip->GetTabHandleAt(2).Get()->opener());
  EXPECT_EQ(tab_model_with_foreground_true_ptr->owning_model(), tabstrip.get());

  // Add a 4th tab using the non foreground option. this is similar to using the
  // AddType NONE, which should not set the active tab and should not inherit
  // the opener.
  tabstrip->AppendTab(std::move(tab_model_with_foreground_false),
                      /*foreground=*/false);
  EXPECT_TRUE(tabstrip->ContainsIndex(3));
  EXPECT_EQ(2, tabstrip->active_index());
  EXPECT_EQ(nullptr, tabstrip->GetTabHandleAt(3).Get()->opener());
  EXPECT_EQ(tab_model_with_foreground_false_ptr->owning_model(),
            tabstrip.get());
}

INSTANTIATE_TEST_SUITE_P(All, TabStripModelTest, ::testing::Bool());
