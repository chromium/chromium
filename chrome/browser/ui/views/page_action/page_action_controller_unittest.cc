// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_controller.h"

#include <map>
#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/metrics/histogram_base.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace page_actions {
namespace {

constexpr int kFirstActionItemId = 0;
constexpr int kSecondActionItemId = 1;
static const PageActionPropertiesMap kTestProperties =
    PageActionPropertiesMap{{
                                kFirstActionItemId,
                                PageActionProperties{
                                    .histogram_name = "Test0",
                                },
                            },
                            {
                                kSecondActionItemId,
                                PageActionProperties{
                                    .histogram_name = "Test1",
                                },
                            }};

const std::u16string kText = u"Text";
const std::u16string kAccessibleName = u"Accessible Name";
const std::u16string kOverrideText = u"Override Text";
const std::u16string kOverrideOne = u"Override One";
const std::u16string kOverrideTwo = u"Override Two";
const std::u16string kOverrideThree = u"Override Three";
const std::u16string kAnotherNewText = u"Another New Text";

const std::u16string kTooltip = u"Tooltip";

using ::actions::ActionItem;
using ::testing::_;
using ::testing::Mock;

using TestPageActionModelObservation =
    ::base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>;

using MockPageActionModelFactory =
    FakePageActionModelFactory<MockPageActionModel>;

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

class PageActionControllerTest : public testing::Test {
 public:
  PageActionControllerTest()
      : properties_provider_(
            PageActionPropertiesMap{{
                                        /*action_id=*/0,
                                        PageActionProperties{
                                            .histogram_name = "Test0",
                                        },
                                    },
                                    {
                                        /*action_id=*/1,
                                        PageActionProperties{
                                            .histogram_name = "Test1",
                                        },
                                    }}) {}

  void SetUp() override {
    pinned_actions_model_ =
        std::make_unique<PinnedToolbarActionsModel>(&profile_);
    controller_ =
        std::make_unique<PageActionControllerImpl>(pinned_actions_model_.get());
    tab_interface_ = std::make_unique<FakeTabInterface>(&profile_);
    tab_interface_->Activate();
  }

  void TearDown() override {
    actions::ActionManager::Get().ResetForTesting();
    controller_.reset();
    pinned_actions_model_.reset();
    tab_interface_.reset();
  }

  PageActionControllerImpl* controller() { return controller_.get(); }

  PinnedToolbarActionsModel* pinned_actions_model() {
    return pinned_actions_model_.get();
  }

  TestingProfile* profile() { return &profile_; }

  FakeTabInterface* tab_interface() { return tab_interface_.get(); }

  std::unique_ptr<ActionItem> BuildActionItem(int action_id) {
    return ActionItem::Builder()
        .SetActionId(action_id)
        .SetVisible(true)
        .SetEnabled(true)
        .Build();
  }

 protected:
  TestPageActionPropertiesProvider properties_provider_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  std::unique_ptr<PageActionControllerImpl> controller_;
  std::unique_ptr<PinnedToolbarActionsModel> pinned_actions_model_;
  std::unique_ptr<ActionItem> action_item_;
  std::unique_ptr<FakeTabInterface> tab_interface_;
};

// Tests adding/removing observers.
TEST_F(PageActionControllerTest, AddAndRemoveObserver) {
  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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

  controller()->Initialize(*tab_interface(), {0, 1}, properties_provider_);

  auto action_item_a = BuildActionItem(0);
  base::CallbackListSubscription subscription_a =
      controller()->CreateActionItemSubscription(action_item_a.get());
  auto action_item_b = BuildActionItem(kSecondActionItemId);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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

  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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
  controller()->Initialize(*tab_interface(), {0}, properties_provider_);
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

TEST_F(PageActionControllerTest, NotifyActionClickedLogsHistogram) {
  base::HistogramTester histogram_tester;

  auto observer = PageActionTestObserver();
  TestPageActionModelObservation observation(&observer);
  controller()->Initialize(*tab_interface(), {kFirstActionItemId},
                           properties_provider_);
  auto action_item = BuildActionItem(kFirstActionItemId);
  base::CallbackListSubscription subscription =
      controller()->CreateActionItemSubscription(action_item.get());
  controller()->AddObserver(0, observation);

  const std::string general_histogram = "PageActionController.Icon.CTR2";
  const std::string specific_histogram = base::StrCat(
      {"PageActionController.",
       properties_provider_.GetProperties(kFirstActionItemId).histogram_name,
       ".Icon.CTR2"});

  histogram_tester.ExpectTotalCount(general_histogram, 0);
  histogram_tester.ExpectTotalCount(specific_histogram, 0);

  // Show the page icon first: Hidden → IconOnly transition (logs one kShown
  // sample).
  controller()->Show(kFirstActionItemId);

  controller()
      ->GetClickCallback(PageActionView::PassKeyForTesting(),
                         kFirstActionItemId)
      .Run(PageActionTrigger::kMouse);

  histogram_tester.ExpectTotalCount(general_histogram, 2);
  histogram_tester.ExpectBucketCount(general_histogram,
                                     PageActionCTREvent::kClicked, 1);
  histogram_tester.ExpectTotalCount(specific_histogram, 2);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kClicked, 1);

  controller()
      ->GetClickCallback(PageActionView::PassKeyForTesting(),
                         kFirstActionItemId)
      .Run(PageActionTrigger::kKeyboard);

  histogram_tester.ExpectTotalCount(general_histogram, 3);
  histogram_tester.ExpectBucketCount(general_histogram,
                                     PageActionCTREvent::kClicked, 2);
  histogram_tester.ExpectTotalCount(specific_histogram, 3);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kClicked, 2);
}

class PageActionControllerMockModelTest : public testing::Test {
 public:
  PageActionControllerMockModelTest()
      : properties_provider_(kTestProperties),
        controller_(
            /*pinned_actions_model=*/nullptr,
            &model_factory_),
        tab_interface_(&profile_) {}

  PageActionControllerImpl& controller() { return controller_; }
  MockPageActionModelFactory& models() { return model_factory_; }
  FakeTabInterface& tab_interface() { return tab_interface_; }

 protected:
  TestPageActionPropertiesProvider properties_provider_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPageActionModelFactory model_factory_;
  PageActionControllerImpl controller_;
  FakeTabInterface tab_interface_;
};

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideText) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  // Set the text override.
  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideText(_, std::optional<std::u16string>(kText)))
      .Times(1);

  controller().OverrideText(kFirstActionItemId, kText);

  // Clear the text override.
  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideText(_, std::optional<std::u16string>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideText(0);
}

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideAccessibleName) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideAccessibleName(
                  _, std::optional<std::u16string>(kAccessibleName)))
      .Times(1);

  controller().OverrideAccessibleName(kFirstActionItemId, kAccessibleName);

  EXPECT_CALL(
      models().Get(kFirstActionItemId),
      SetOverrideAccessibleName(_, std::optional<std::u16string>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideAccessibleName(kFirstActionItemId);
}

TEST_F(PageActionControllerMockModelTest, TabActivation) {
  tab_interface().Deactivate();
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId), SetTabActive(_, true)).Times(1);
  tab_interface().Activate();
}

TEST_F(PageActionControllerMockModelTest, TabDeactivation) {
  tab_interface().Activate();
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId), SetTabActive(_, false))
      .Times(1);
  tab_interface().Deactivate();
}

TEST_F(PageActionControllerMockModelTest, ShowSuggestionChip) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetShouldShowSuggestionChip(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetSuggestionChipConfig(_,
                                      SuggestionChipConfig{
                                          .should_animate = true,
                                          .should_announce_chip = false,
                                      }))
      .Times(1);
  controller().ShowSuggestionChip(kFirstActionItemId);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetShouldShowSuggestionChip(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetSuggestionChipConfig(_,
                                      SuggestionChipConfig{
                                          .should_animate = false,
                                          .should_announce_chip = true,
                                      }))
      .Times(1);
  controller().ShowSuggestionChip(
      kFirstActionItemId,
      {.should_animate = false, .should_announce_chip = true});

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetShouldShowSuggestionChip(_, false))
      .Times(1);
  controller().HideSuggestionChip(kFirstActionItemId);
}

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideImage) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  ui::ImageModel override_image =
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/32));

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideImage(_, std::optional<ui::ImageModel>(override_image),
                               PageActionColorSource::kForeground))
      .Times(1);
  controller().OverrideImage(kFirstActionItemId, override_image);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideImage(_, std::optional<ui::ImageModel>(std::nullopt),
                               PageActionColorSource::kForeground))
      .Times(1);
  controller().ClearOverrideImage(kFirstActionItemId);
}

TEST_F(PageActionControllerMockModelTest, OverrideImageWithColorSource) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  ui::ImageModel override_image =
      ui::ImageModel::FromImageSkia(gfx::test::CreateImageSkia(/*size=*/32));

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetOverrideImage(_, std::optional<ui::ImageModel>(override_image),
                               PageActionColorSource::kCascadingAccent))
      .Times(1);
  controller().OverrideImage(kFirstActionItemId, override_image,
                             PageActionColorSource::kCascadingAccent);
}

TEST_F(PageActionControllerMockModelTest, SetAndClearOverrideTooltip) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(
      models().Get(kFirstActionItemId),
      SetOverrideTooltip(_, std::optional<std::u16string>(kOverrideText)))
      .Times(1);
  controller().OverrideTooltip(kFirstActionItemId, kOverrideText);

  EXPECT_CALL(
      models().Get(kFirstActionItemId),
      SetOverrideTooltip(_, std::optional<std::u16string>(std::nullopt)))
      .Times(1);
  controller().ClearOverrideTooltip(kFirstActionItemId);
}

TEST_F(PageActionControllerMockModelTest, ShouldForciblyHidePageActions) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetIsSuppressedByOmnibox(_, /*is_suppressed*/ true))
      .Times(1);

  controller().SetShouldHidePageActions(true);

  EXPECT_CALL(models().Get(kFirstActionItemId),
              SetIsSuppressedByOmnibox(_, /*is_suppressed*/ false))
      .Times(1);

  controller().SetShouldHidePageActions(false);
}

TEST_F(PageActionControllerMockModelTest, ActivityCounter) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  // Add first activity scope. Model becomes active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  auto activity1 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Add second activity scope. Model remains active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  auto activity2 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Destroy first activity scope. Model remains active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  activity1.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Destroy second activity scope. Model becomes inactive.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, false))
      .Times(1);
  activity2.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
}

TEST_F(PageActionControllerMockModelTest, ActivityCounterMultipleActions) {
  controller().Initialize(tab_interface(),
                          {kFirstActionItemId, kSecondActionItemId},
                          properties_provider_);

  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, _))
      .Times(0);
  auto activity1 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  // Add activity for second action.
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  auto activity2 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kSecondActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  // Destroy activity for second action.
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, false))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  activity2.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  // Destroy one activity for first action.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, false))
      .Times(1);
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, _))
      .Times(0);
  activity1.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));
}

TEST_F(PageActionControllerMockModelTest, ActivityCounterAssignmentOperator) {
  controller().Initialize(tab_interface(),
                          {kFirstActionItemId, kSecondActionItemId},
                          properties_provider_);
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, _))
      .Times(0);
  ScopedPageActionActivity activity1 =
      controller().AddActivity(kFirstActionItemId);
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  // Add activity for second action.
  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, true))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  std::optional<ScopedPageActionActivity> activity2 =
      controller().AddActivity(kSecondActionItemId);
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  EXPECT_CALL(models().Get(kSecondActionItemId), SetActionActive(_, false))
      .Times(1);
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  activity2 = std::move(activity1);
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kSecondActionItemId));

  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, false))
      .Times(1);
  activity2.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
}

TEST_F(PageActionControllerMockModelTest,
       ActivityResetsOnControllerDestruction) {
  controller().Initialize(tab_interface(), {kFirstActionItemId},
                          properties_provider_);

  // Add first activity scope. Model becomes active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  auto activity1 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Add second activity scope. Model remains active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, true))
      .Times(1);
  auto activity2 = std::make_unique<ScopedPageActionActivity>(
      controller().AddActivity(kFirstActionItemId));
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Destroy first activity scope. Model remains active.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, _)).Times(0);
  activity1.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));

  // Destroy second activity scope. Model becomes inactive.
  EXPECT_CALL(models().Get(kFirstActionItemId), SetActionActive(_, false))
      .Times(1);
  activity2.reset();
  Mock::VerifyAndClearExpectations(&models().Get(kFirstActionItemId));
}

}  // namespace
}  // namespace page_actions
