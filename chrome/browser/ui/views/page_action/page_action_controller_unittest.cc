// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/views/page_action/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace page_actions {
namespace {

constexpr int kActionItemId = 0;

const std::u16string kText = u"Text";
const std::u16string kOverrideText = u"Override Text";
const std::u16string kOverrideOne = u"Override One";
const std::u16string kOverrideTwo = u"Override Two";
const std::u16string kOverrideThree = u"Override Three";
const std::u16string kAnotherNewText = u"Another New Text";

const std::u16string kTooltip = u"Tooltip";

using ::actions::ActionItem;
using ::testing::_;

using TestPageActionModelObservation =
    ::base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>;

class FakeTabInterface : public tabs::MockTabInterface {
 public:
  ~FakeTabInterface() override = default;

  base::CallbackListSubscription RegisterDidActivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override {
    return activation_callbacks_.Add(cb);
  }

  base::CallbackListSubscription RegisterWillDeactivate(
      base::RepeatingCallback<void(TabInterface*)> cb) override {
    return deactivation_callbacks_.Add(cb);
  }

  void Activate() {
    is_activated_ = true;
    activation_callbacks_.Notify(this);
  }

  void Deactivate() {
    is_activated_ = false;
    deactivation_callbacks_.Notify(this);
  }

  bool IsActivated() const override { return is_activated_; }

 private:
  bool is_activated_ = false;
  base::RepeatingCallbackList<void(TabInterface*)> activation_callbacks_;
  base::RepeatingCallbackList<void(TabInterface*)> deactivation_callbacks_;
};

class PageActionTestObserver : public PageActionModelObserver {
 public:
  PageActionTestObserver() = default;
  ~PageActionTestObserver() override = default;

  void OnPageActionModelChanged(
      const PageActionModelInterface& model) override {
    ++model_change_count_;

    visible_ = model.GetVisible();
    text_ = model.GetText();
    tooltip_text_ = model.GetTooltipText();
    image_ = model.GetImage();
  }

  int model_change_count() const { return model_change_count_; }

  bool visible() const { return visible_; }
  const std::u16string& text() const { return text_; }
  const std::u16string& tooltip_text() const { return tooltip_text_; }
  const ui::ImageModel& image() const { return image_; }

 private:
  // Model data.
  bool visible_ = false;
  std::u16string text_;
  std::u16string tooltip_text_;
  ui::ImageModel image_;

  int model_change_count_ = 0;
};

class PageActionControllerTest : public ::testing::Test {
 public:
  PageActionControllerTest() = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(profile_.get());
    controller_ =
        std::make_unique<PageActionController>(pinned_actions_model_.get());
    tab_interface_ = std::make_unique<FakeTabInterface>();
    tab_interface_->Activate();
  }

  void TearDown() override {
    actions::ActionManager::Get().ResetForTesting();
    controller_.reset();
    pinned_actions_model_.reset();
    profile_.reset();
    tab_interface_.reset();
  }

  PageActionController* controller() { return controller_.get(); }
  PinnedToolbarActionsModel* pinned_actions_model() {
    return pinned_actions_model_.get();
  }
  TestingProfile* profile() { return profile_.get(); }

  FakeTabInterface* tab_interface() { return tab_interface_.get(); }

  std::unique_ptr<ActionItem> BuildActionItem(int action_id) {
    return ActionItem::Builder()
        .SetActionId(action_id)
        .SetVisible(true)
        .SetEnabled(true)
        .Build();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<PageActionController> controller_;
  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ActionItem> action_item_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
};

// Tests adding/removing observers.
TEST_F(PageActionControllerTest, AddAndRemoveObserver) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  controller()->AddObserver(0, observation);
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());

  controller()->Show(0);
  EXPECT_TRUE(observer.visible());

  observation.Reset();
  controller()->Hide(0);
  EXPECT_TRUE(observer.visible());
}

// Tests that calling Show/HidePageAction is reflected in the model.
TEST_F(PageActionControllerTest, ShowAndHidePageAction) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  EXPECT_EQ(0, observer.model_change_count());
  controller()->Show(0);
  EXPECT_EQ(1, observer.model_change_count());
  EXPECT_TRUE(observer.visible());

  controller()->Show(0);
  EXPECT_EQ(1, observer.model_change_count());
  EXPECT_TRUE(observer.visible());

  controller()->Hide(0);
  EXPECT_EQ(2, observer.model_change_count());
  EXPECT_FALSE(observer.visible());
}

// Tests that calling Show/HidePageAction will show/hide update the correct
// model.
TEST_F(PageActionControllerTest, ShowAndHidePageActionUpdatesCorrectModel) {
  auto observer_a = PageActionTestObserver();
  auto observer_b = PageActionTestObserver();
  TestPageActionModelObservation observation_a(&observer_a);
  TestPageActionModelObservation observation_b(&observer_b);

  controller()->Initialize(*tab_interface(), {0, 1});

  auto action_item_a = BuildActionItem(0);
  base::CallbackListSubscription subscription_a =
      controller()->CreateActionItemSubscription(action_item_a.get());
  auto action_item_b = BuildActionItem(1);
  base::CallbackListSubscription subscription_b =
      controller()->CreateActionItemSubscription(action_item_b.get());

  controller()->AddObserver(0, observation_a);
  controller()->AddObserver(1, observation_b);

  controller()->Show(0);
  EXPECT_TRUE(observer_a.visible());
  EXPECT_FALSE(observer_b.visible());

  controller()->Show(1);
  EXPECT_TRUE(observer_a.visible());
  EXPECT_TRUE(observer_b.visible());

  controller()->Hide(0);
  EXPECT_FALSE(observer_a.visible());
  EXPECT_TRUE(observer_b.visible());
}

TEST_F(PageActionControllerTest, ActionItemPropertiesUpdateModel) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  action_item->SetText(kText);
  action_item->SetTooltipText(kTooltip);

  EXPECT_EQ(kText, observer.text());
  EXPECT_EQ(kTooltip, observer.tooltip_text());
}

TEST_F(PageActionControllerTest, ShowIfNotPinned) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  auto action_item = BuildActionItem(0);
  controller()->Initialize(*tab_interface(), {0});
  controller()->AddObserver(0, observation);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());

  PinnedToolbarActionsModel* pinned_actions = pinned_actions_model();
  EXPECT_EQ(1, observer.model_change_count());

  // Pin the action item. Nothing should happen.
  pinned_actions->UpdatePinnedState(0, true);
  EXPECT_EQ(2, observer.model_change_count());
  EXPECT_FALSE(observer.visible());

  controller()->Show(0);
  EXPECT_EQ(3, observer.model_change_count());
  EXPECT_FALSE(observer.visible());

  // Unpin the action item. The page action should now be visible.
  pinned_actions->UpdatePinnedState(0, false);
  EXPECT_EQ(4, observer.model_change_count());
  EXPECT_TRUE(observer.visible());

  controller()->Hide(0);
  EXPECT_EQ(5, observer.model_change_count());
  EXPECT_FALSE(observer.visible());
}

TEST_F(PageActionControllerTest, ActionPinnedAtInitialization) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  auto action_item = BuildActionItem(0);
  PinnedToolbarActionsModel* pinned_actions = pinned_actions_model();
  pinned_actions->UpdatePinnedState(0, true);

  controller()->Initialize(*tab_interface(), {0});
  controller()->AddObserver(0, observation);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());

  controller()->Show(0);
  EXPECT_FALSE(observer.visible());

  // Unpin the action item. The page action should now be visible.
  pinned_actions->UpdatePinnedState(0, false);
  EXPECT_TRUE(observer.visible());
}

TEST_F(PageActionControllerTest, PinnedActionPrefChanged) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  auto action_item = BuildActionItem(0);
  controller()->Initialize(*tab_interface(), {0});
  controller()->AddObserver(0, observation);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());

  controller()->Show(0);
  EXPECT_TRUE(observer.visible());

  {
    ScopedListPrefUpdate pref_update(profile()->GetPrefs(),
                                     prefs::kPinnedActions);
    pref_update.Get().Append(actions::ActionIdMap::ActionIdToString(0).value());
  }
  EXPECT_FALSE(observer.visible());

  {
    ScopedListPrefUpdate pref_update(profile()->GetPrefs(),
                                     prefs::kPinnedActions);
    pref_update.Get().clear();
  }
  EXPECT_TRUE(observer.visible());
}

TEST_F(PageActionControllerTest, OverrideText) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  action_item->SetText(kText);

  controller()->OverrideText(0, kOverrideText);
  EXPECT_EQ(kOverrideText, observer.text());
}

TEST_F(PageActionControllerTest, UpdateActionItemTextWithOverrideText) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  action_item->SetText(kText);

  controller()->OverrideText(0, kOverrideText);
  EXPECT_EQ(kOverrideText, observer.text());

  action_item->SetText(kAnotherNewText);
  // The override text should still take precedence.
  EXPECT_EQ(kOverrideText, observer.text());
}

TEST_F(PageActionControllerTest, ClearOverrideText) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0});
  auto action_item = BuildActionItem(0);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  action_item->SetText(kText);
  controller()->OverrideText(0, kOverrideText);
  controller()->ClearOverrideText(0);

  // We should revert to the ActionItem text.
  EXPECT_EQ(kText, observer.text());
}

class MockPageActionModelFactory : public PageActionModelFactory {
 public:
  // Interface used by PageActionController to create a model.
  std::unique_ptr<PageActionModelInterface> Create(int action_id) override {
    auto model = std::make_unique<MockPageActionModel>();
    model_map_.emplace(action_id, model.get());
    return model;
  }

  // Model getter for tests to set expectations.
  MockPageActionModel& Get(int action_id) {
    auto id_to_model = model_map_.find(action_id);
    CHECK(id_to_model != model_map_.end());
    CHECK_NE(id_to_model->second, nullptr);
    return *id_to_model->second;
  }

 private:
  std::map<actions::ActionId, MockPageActionModel*> model_map_;
};

class PageActionControllerMockModelTest : public ::testing::Test {
 public:
  PageActionControllerMockModelTest()
      : controller_(/*pinned_actions_model=*/nullptr, &model_factory_) {}

  PageActionController& controller() { return controller_; }
  MockPageActionModelFactory& models() { return model_factory_; }
  FakeTabInterface& tab_interface() { return tab_interface_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  MockPageActionModelFactory model_factory_;
  PageActionController controller_;
  FakeTabInterface tab_interface_;
};

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideText) {
  controller().Initialize(tab_interface(), {kActionItemId});

  // Set the text override.
  EXPECT_CALL(models().Get(kActionItemId),
              SetOverrideText(_, std::optional<std::u16string>(kText)))
      .Times(1);

  controller().OverrideText(kActionItemId, kText);

  // Clear the text override.
  EXPECT_CALL(models().Get(kActionItemId),
              SetOverrideText(_, std::optional<std::u16string>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideText(0);
}

TEST_F(PageActionControllerMockModelTest, TabActivation) {
  tab_interface().Deactivate();
  controller().Initialize(tab_interface(), {kActionItemId});

  EXPECT_CALL(models().Get(kActionItemId), SetTabActive(_, true)).Times(1);
  tab_interface().Activate();
}

TEST_F(PageActionControllerMockModelTest, TabDeactivation) {
  tab_interface().Activate();
  controller().Initialize(tab_interface(), {kActionItemId});

  EXPECT_CALL(models().Get(kActionItemId), SetTabActive(_, false)).Times(1);
  tab_interface().Deactivate();
}

TEST_F(PageActionControllerMockModelTest, ShowSuggestionChip) {
  controller().Initialize(tab_interface(), {kActionItemId});

  EXPECT_CALL(models().Get(kActionItemId), SetShowSuggestionChip(_, true))
      .Times(1);
  controller().ShowSuggestionChip(kActionItemId);

  EXPECT_CALL(models().Get(kActionItemId), SetShowSuggestionChip(_, false))
      .Times(1);
  controller().HideSuggestionChip(kActionItemId);
}

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideImage) {
  controller().Initialize(tab_interface(), {kActionItemId});

  ui::ImageModel override_image =
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/32));

  EXPECT_CALL(
      models().Get(kActionItemId),
      SetOverrideImage(_, std::optional<ui::ImageModel>(override_image)))
      .Times(1);
  controller().OverrideImage(kActionItemId, override_image);

  EXPECT_CALL(models().Get(kActionItemId),
              SetOverrideImage(_, std::optional<ui::ImageModel>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideImage(kActionItemId);
}

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideTooltip) {
  controller().Initialize(tab_interface(), {kActionItemId});

  EXPECT_CALL(
      models().Get(kActionItemId),
      SetOverrideTooltip(_, std::optional<std::u16string>(kOverrideText)))
      .Times(1);
  controller().OverrideTooltip(kActionItemId, kOverrideText);

  EXPECT_CALL(
      models().Get(kActionItemId),
      SetOverrideTooltip(_, std::optional<std::u16string>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideTooltip(kActionItemId);
}

}  // namespace
}  // namespace page_actions
