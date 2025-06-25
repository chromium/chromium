// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_observer.h"

#include <optional>
#include <string>

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/test_support/noop_page_action_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace page_actions {
namespace {

static const std::u16string kTestLabel = u"Test Label";
static const std::u16string kTestTooltip = u"Test Tooltip";

static constexpr actions::ActionId kTestPageActionId = 0;
static const PageActionPropertiesMap kTestProperties = PageActionPropertiesMap{
    {
        kTestPageActionId,
        PageActionProperties{
            .histogram_name = "Test",
        },
    },
};

PageActionState ExpectedShowingState() {
  PageActionState expected_state;
  expected_state.action_id = kTestPageActionId;
  expected_state.showing = true;
  expected_state.tooltip = std::make_optional(kTestTooltip);
  return expected_state;
}

PageActionState ExpectedHiddenState() {
  PageActionState expected_state;
  expected_state.action_id = kTestPageActionId;
  return expected_state;
}

PageActionState ExpectedShowingChipState() {
  PageActionState expected_state;
  expected_state.action_id = kTestPageActionId;
  expected_state.showing = true;
  expected_state.chip_showing = true;
  expected_state.label = std::make_optional(kTestLabel);
  expected_state.tooltip = std::make_optional(kTestTooltip);
  return expected_state;
}

class MockPageActionObserver : public PageActionObserver {
 public:
  MockPageActionObserver() : PageActionObserver(kTestPageActionId) {}
  ~MockPageActionObserver() override = default;

  MOCK_METHOD(void,
              OnPageActionIconShown,
              (const PageActionState&),
              (override));
  MOCK_METHOD(void,
              OnPageActionIconHidden,
              (const PageActionState&),
              (override));
  MOCK_METHOD(void,
              OnPageActionChipShown,
              (const PageActionState&),
              (override));
  MOCK_METHOD(void,
              OnPageActionChipHidden,
              (const PageActionState&),
              (override));
};

class NotifierPageActionModel : public MockPageActionModel {
 public:
  NotifierPageActionModel() = default;
  ~NotifierPageActionModel() override {
    observer_list_.Notify(
        &PageActionModelObserver::OnPageActionModelWillBeDeleted, *this);
  }

  void AddObserver(PageActionModelObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(PageActionModelObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void NotifyChanged() {
    observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                          *this);
  }

 private:
  base::ObserverList<PageActionModelObserver> observer_list_;
};

using MockPageActionModelFactory =
    FakePageActionModelFactory<NotifierPageActionModel>;

class PageActionObserverTest : public ::testing::Test {
 public:
  PageActionObserverTest() : tab_(nullptr) {}

  void SetUp() override {
    controller_ = std::make_unique<PageActionControllerImpl>(
        nullptr, &model_factory_, &metrics_factory_);
    controller_->Initialize(tab_, {kTestPageActionId},
                            TestPageActionPropertiesProvider(kTestProperties));
    ON_CALL(model_factory_.Get(kTestPageActionId), GetTooltipText())
        .WillByDefault(ReturnRef(kTestTooltip));
    ON_CALL(model_factory_.Get(kTestPageActionId), GetText())
        .WillByDefault(ReturnRef(kTestLabel));
  }

  MockPageActionObserver& observer() { return observer_; }
  PageActionController& controller() { return *controller_; }
  MockPageActionModelFactory& factory() { return model_factory_; }

 private:
  MockPageActionObserver observer_;
  FakeTabInterface tab_;
  MockPageActionModelFactory model_factory_;
  NoopPageActionMetricsRecorderFactory metrics_factory_;
  std::unique_ptr<PageActionControllerImpl> controller_;
};

TEST_F(PageActionObserverTest, OnPageActionIconShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(false));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  EXPECT_CALL(observer(), OnPageActionIconShown(ExpectedShowingState()))
      .Times(1);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionIconHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(Return(false));
  EXPECT_CALL(observer(), OnPageActionIconHidden(ExpectedHiddenState()))
      .Times(1);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconHidden(_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionChipShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(false));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(false));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(true));
  EXPECT_CALL(observer(), OnPageActionChipShown(ExpectedShowingChipState()))
      .Times(1);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionChipHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(false));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(false));
  EXPECT_CALL(observer(), OnPageActionChipHidden(ExpectedShowingState()))
      .Times(1);
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest,
       OnPageActionChipVisibilityConsidersIconVisibility) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(false));
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(1);
  model.NotifyChanged();

  ON_CALL(model, GetVisible()).WillByDefault(Return(false));
  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(1);
  model.NotifyChanged();
}

// When the icon is visible and the suggestion-chip request is true, flipping
// ShouldShowSuggestionChip() from false → true should be reported as “chip
// shown”.
TEST_F(PageActionObserverTest, ChipExpandedTrueTriggersChipShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);

  // Icon visible, chip requested, but not yet expanded.
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(false));

  observer().RegisterAsPageActionObserver(controller());

  // Flip to expanded.
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(true));
  EXPECT_CALL(observer(), OnPageActionChipShown(ExpectedShowingChipState()))
      .Times(1);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);

  model.NotifyChanged();

  // Further notifications with no state change → no callbacks.
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);
  model.NotifyChanged();
}

// When the chip is currently expanded, collapsing it (true → false) – while the
// request to show a chip remains – must be reported as “chip hidden”.
TEST_F(PageActionObserverTest, ChipExpandedFalseTriggersChipHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);

  // Icon + expanded chip showing.
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(true));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(true));

  observer().RegisterAsPageActionObserver(controller());

  // Collapse the chip.
  ON_CALL(model, ShouldShowSuggestionChip()).WillByDefault(Return(false));
  ON_CALL(model, IsChipShowing()).WillByDefault(Return(false));
  EXPECT_CALL(observer(), OnPageActionChipHidden(ExpectedShowingState()))
      .Times(1);
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);

  model.NotifyChanged();

  // No more callbacks on further unchanged notifications.
  EXPECT_CALL(observer(), OnPageActionChipShown(_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(_)).Times(0);
  model.NotifyChanged();
}

// Ensures that the initial state of a page action model is applied when
// registering as the observer. Further change notifications shouldn't
// have an effect.
TEST_F(PageActionObserverTest, PageActionInitialStateShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(true));
  observer().RegisterAsPageActionObserver(controller());

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, PageActionInitialStateHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(Return(false));
  observer().RegisterAsPageActionObserver(controller());

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(_)).Times(0);
  model.NotifyChanged();
}

}  // namespace
}  // namespace page_actions
