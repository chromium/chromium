// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"

namespace {
// A simple observer that tracks the number of times certain events occur.
class PinnedToolbarActionsModelTestObserver
    : public PinnedToolbarActionsModel::Observer {
 public:
  explicit PinnedToolbarActionsModelTestObserver(
      PinnedToolbarActionsModel* model)
      : model_(model) {
    model_->AddObserver(this);
  }

  PinnedToolbarActionsModelTestObserver(
      const PinnedToolbarActionsModelTestObserver&) = delete;
  PinnedToolbarActionsModelTestObserver& operator=(
      const PinnedToolbarActionsModelTestObserver&) = delete;

  ~PinnedToolbarActionsModelTestObserver() override {
    model_->RemoveObserver(this);
  }

  int inserted_count() const { return inserted_count_; }
  int removed_count() const { return removed_count_; }
  int moved_to_index() const { return moved_to_index_; }

  actions::ActionId last_changed_action() const { return last_changed_action_; }

  const std::vector<actions::ActionId>& last_changed_ids() const {
    return last_changed_ids_;
  }

 private:
  // PinnedToolbarActionsModel::Observer:
  void OnActionAddedLocally(const actions::ActionId& action_id) override {
    ++inserted_count_;
    last_changed_action_ = action_id;
  }

  void OnActionRemovedLocally(const actions::ActionId& action_id) override {
    ++removed_count_;
    last_changed_action_ = action_id;
  }

  // Signals that the given action with `id` has been moved in the model.
  void OnActionMovedLocally(const actions::ActionId& id,
                            int from_index,
                            int to_index) override {
    moved_to_index_ = to_index;
    last_changed_action_ = id;
  }

  void OnActionsChanged() override {
    last_changed_ids_ = model_->PinnedActionIds();
  }

  const raw_ptr<PinnedToolbarActionsModel> model_;

  int inserted_count_ = 0;
  int removed_count_ = 0;
  int moved_to_index_ = -1;

  actions::ActionId last_changed_action_ = -1;
  std::vector<actions::ActionId> last_changed_ids_;
};
}  // namespace

class PinnedToolbarActionsModelBrowserTest : public InProcessBrowserTest {
 public:
  PinnedToolbarActionsModelBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {companion::features::internal::kSidePanelCompanion}, {});
  }

  PinnedToolbarActionsModelBrowserTest(
      const PinnedToolbarActionsModelBrowserTest&) = delete;
  PinnedToolbarActionsModelBrowserTest& operator=(
      const PinnedToolbarActionsModelBrowserTest&) = delete;

  ~PinnedToolbarActionsModelBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    model_ =
        PinnedToolbarActionsModelFactory::GetForProfile(browser()->profile());
    model_observer_ =
        std::make_unique<PinnedToolbarActionsModelTestObserver>(model_);
  }

  void TearDownOnMainThread() override {
    model_observer_.reset();
    model_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  PinnedToolbarActionsModel* model() { return model_; }

  const PinnedToolbarActionsModelTestObserver* observer() const {
    return model_observer_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<PinnedToolbarActionsModel> model_;
  std::unique_ptr<PinnedToolbarActionsModelTestObserver> model_observer_;
};

// Verify that we are able to add new pinned actions to the model and that
// it updates the prefs object accordingly.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest, PinActions) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  EXPECT_EQ(kActionSidePanelShowBookmarks, observer()->last_changed_action());

  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);
  EXPECT_EQ(kActionSidePanelShowReadingList, observer()->last_changed_action());

  model()->UpdatePinnedState(kActionSidePanelShowHistoryCluster,
                             /*should_pin=*/true);
  EXPECT_EQ(kActionSidePanelShowHistoryCluster,
            observer()->last_changed_action());

  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());

  // Verify all actions ids were added to the model and that the prefs object
  // maintains insertion order.
  const base::Value::List& list =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);

  ASSERT_EQ(4u, list.size());

  ASSERT_EQ("kActionShowChromeLabs", list[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list[1].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list[2].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list[3].GetString());
}

// Verify that we are able to remove pinned actions from the model and that
// it updates the prefs object accordingly.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest, UnpinActions) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowHistoryCluster,
                             /*should_pin=*/true);

  // Expect unpinning the second ActionId will remove it from the model and the
  // prefs object.
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/false);
  EXPECT_EQ(1, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());

  // Verify only the second ActionId was removed.
  const base::Value::List& list =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ("kActionShowChromeLabs", list[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list[1].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list[2].GetString());
}

// Verify that we are able to move pinned actions in the model and that
// it updates the prefs object accordingly.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       MovePinnedActions) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowHistoryCluster,
                             /*should_pin=*/true);

  // Expect moving the second action will put it at the end of the list.
  model()->MovePinnedAction(kActionSidePanelShowReadingList, 3);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(3, observer()->moved_to_index());

  // Verify kActionCopy was moved to the end of the list which should be
  // index 2.
  const base::Value::List& list_1 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_1.size());
  ASSERT_EQ("kActionShowChromeLabs", list_1[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_1[1].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_1[2].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_1[3].GetString());

  // Expect that we can move the first action after the second action correctly.
  model()->MovePinnedAction(kActionSidePanelShowBookmarks, 2);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(2, observer()->moved_to_index());

  // kActionSidePanelShowBookmarks was move to the end.
  const base::Value::List& list_2 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_2.size());
  ASSERT_EQ("kActionShowChromeLabs", list_2[0].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_2[1].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_2[2].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_2[3].GetString());

  // Expect that moving the kActionCopy action to the beginning of the list will
  // place it in front of the current first element.
  model()->MovePinnedAction(kActionSidePanelShowReadingList, 1);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(1, observer()->moved_to_index());

  // Verify kActionCopy was moved to index 0.
  const base::Value::List& list_3 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_3.size());
  ASSERT_EQ("kActionShowChromeLabs", list_3[0].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_3[1].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_3[2].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_3[3].GetString());
}

// Verify that trying to move a pinned action out of bounds will do nothing.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       MovePinnedActionsOutOfBoundsDoesNothing) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowHistoryCluster,
                             /*should_pin=*/true);

  // Expect that moving an Action out of bounds at the end of the list does
  // nothing.
  model()->MovePinnedAction(kActionSidePanelShowReadingList, 4);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(-1, observer()->moved_to_index());

  // Verify the action did not move.
  const base::Value::List& list_1 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_1.size());
  ASSERT_EQ("kActionShowChromeLabs", list_1[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_1[1].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_1[2].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_1[3].GetString());

  // Expect that moving an action out of bounds before the list does nothing.
  model()->MovePinnedAction(kActionSidePanelShowBookmarks, -1);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(-1, observer()->moved_to_index());

  // Verify the action did not move.
  const base::Value::List& list_2 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_2.size());

  ASSERT_EQ("kActionShowChromeLabs", list_2[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_2[1].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_2[2].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_2[3].GetString());
}

// Verify that trying to move a pinned action out of bounds will do nothing.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       MoveUnpinnedActionDoesNothing) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);

  // Expect that moving an action which is not added to the model does nothing.
  model()->MovePinnedAction(kActionSidePanelShowHistoryCluster, 3);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(2, observer()->inserted_count());
  EXPECT_EQ(-1, observer()->moved_to_index());

  // Verify nothing changed.
  const base::Value::List& list_1 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(3u, list_1.size());
  ASSERT_EQ("kActionShowChromeLabs", list_1[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_1[1].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_1[2].GetString());
}

// Verify that trying to move a pinned action to its current index does nothing.
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       MovePinnedActionToSameIndexDoesNothing) {
  // Pin 3 ActionIds.
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowReadingList,
                             /*should_pin=*/true);
  model()->UpdatePinnedState(kActionSidePanelShowHistoryCluster,
                             /*should_pin=*/true);

  // Expect that moving an action to the same index does nothing.
  model()->MovePinnedAction(kActionSidePanelShowHistoryCluster, 3);
  EXPECT_EQ(0, observer()->removed_count());
  EXPECT_EQ(3, observer()->inserted_count());
  EXPECT_EQ(-1, observer()->moved_to_index());

  // Verify no action moved.
  const base::Value::List& list_1 =
      browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
  ASSERT_EQ(4u, list_1.size());
  ASSERT_EQ("kActionShowChromeLabs", list_1[0].GetString());
  EXPECT_EQ("kActionSidePanelShowBookmarks", list_1[1].GetString());
  EXPECT_EQ("kActionSidePanelShowReadingList", list_1[2].GetString());
  EXPECT_EQ("kActionSidePanelShowHistoryCluster", list_1[3].GetString());
}

IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       ResetToDefaultResetsToDefault) {
  EXPECT_TRUE(model()->IsDefault());
  EXPECT_FALSE(model()->Contains(kActionSidePanelShowBookmarks));
  EXPECT_TRUE(model()->Contains(kActionShowChromeLabs));

  model()->UpdatePinnedState(kActionShowChromeLabs, false);
  model()->UpdatePinnedState(kActionSidePanelShowBookmarks, true);

  EXPECT_FALSE(model()->IsDefault());
  EXPECT_TRUE(model()->Contains(kActionSidePanelShowBookmarks));
  EXPECT_FALSE(model()->Contains(kActionShowChromeLabs));

  model()->ResetToDefault();

  EXPECT_TRUE(model()->IsDefault());
  EXPECT_FALSE(model()->Contains(kActionSidePanelShowBookmarks));
  EXPECT_TRUE(model()->Contains(kActionShowChromeLabs));
}

// Verify that the search companion updates the model and prefs object
// appropriately.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PinnedToolbarActionsModelBrowserTest,
                       MigratingSearchCompanionUpdatesModel) {
  // Verify nothing happens if the migration already happened.
  {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPinnedSearchCompanionMigrationComplete, true);
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSidePanelCompanionEntryPinnedToToolbar, true);
    EXPECT_EQ(0, observer()->removed_count());
    EXPECT_EQ(0, observer()->inserted_count());
    EXPECT_EQ(-1, observer()->moved_to_index());

    companion::UpdateCompanionDefaultPinnedToToolbarState(browser()->profile());

    const base::Value::List& list_1 =
        browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
    EXPECT_EQ(1u, list_1.size());
  }

  // Verify nothing happens if the search companion is not pinned.
  {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPinnedSearchCompanionMigrationComplete, false);
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSidePanelCompanionEntryPinnedToToolbar, false);
    EXPECT_EQ(0, observer()->removed_count());
    EXPECT_EQ(0, observer()->inserted_count());
    EXPECT_EQ(-1, observer()->moved_to_index());

    companion::UpdateCompanionDefaultPinnedToToolbarState(browser()->profile());

    const base::Value::List& list_1 =
        browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);
    EXPECT_EQ(1u, list_1.size());
  }

  // Verify the migration updates the model if search companion is pinned.
  {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPinnedSearchCompanionMigrationComplete, false);
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kSidePanelCompanionEntryPinnedToToolbar, true);
    EXPECT_EQ(0, observer()->removed_count());
    EXPECT_EQ(0, observer()->inserted_count());
    EXPECT_EQ(-1, observer()->moved_to_index());

    companion::UpdateCompanionDefaultPinnedToToolbarState(browser()->profile());

    const base::Value::List& list_1 =
        browser()->profile()->GetPrefs()->GetList(prefs::kPinnedActions);

    ASSERT_EQ(2u, list_1.size());
    EXPECT_EQ(1, observer()->inserted_count());

    const std::optional<std::string>& search_companion_string =
        actions::ActionIdMap::ActionIdToString(
            kActionSidePanelShowSearchCompanion);
    ASSERT_EQ("kActionShowChromeLabs", list_1[0].GetString());
    EXPECT_EQ(search_companion_string, list_1[1].GetString());
  }
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)

// TODO(dljames): Write tests for guest and incognito mode profile that check
// that we cannot modify the model at all.
