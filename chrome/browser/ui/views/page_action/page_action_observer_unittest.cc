// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_observer.h"

#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/test_support/noop_page_action_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/test_support/test_page_action_properties_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_actions {
namespace {

static constexpr actions::ActionId kTestPageActionId = 0;
static const PageActionPropertiesMap kTestProperties = PageActionPropertiesMap{
    {
        kTestPageActionId,
        PageActionProperties{
            .histogram_name = "Test",
            .is_ephemeral = true,
        },
    },
};

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
    controller_ = std::make_unique<PageActionController>(
        nullptr, &model_factory_, &metrics_factory_);
    controller_->Initialize(tab_, {kTestPageActionId},
                            TestPageActionPropertiesProvider(kTestProperties));
  }

  MockPageActionObserver& observer() { return observer_; }
  PageActionController& controller() { return *controller_; }
  MockPageActionModelFactory& factory() { return model_factory_; }

 private:
  MockPageActionObserver observer_;
  FakeTabInterface tab_;
  MockPageActionModelFactory model_factory_;
  NoopPageActionMetricsRecorderFactory metrics_factory_;
  std::unique_ptr<PageActionController> controller_;
};

TEST_F(PageActionObserverTest, OnPageActionIconShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(false));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  EXPECT_CALL(observer(), OnPageActionIconShown(testing::_)).Times(1);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(testing::_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionIconHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(false));
  EXPECT_CALL(observer(), OnPageActionIconHidden(testing::_)).Times(1);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconHidden(testing::_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionChipShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  ON_CALL(model, GetShowSuggestionChip()).WillByDefault(testing::Return(false));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetShowSuggestionChip()).WillByDefault(testing::Return(true));
  EXPECT_CALL(observer(), OnPageActionChipShown(testing::_)).Times(1);
  EXPECT_CALL(observer(), OnPageActionChipHidden(testing::_)).Times(0);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipShown(testing::_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(testing::_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, OnPageActionChipHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  ON_CALL(model, GetShowSuggestionChip()).WillByDefault(testing::Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetShowSuggestionChip()).WillByDefault(testing::Return(false));
  EXPECT_CALL(observer(), OnPageActionChipHidden(testing::_)).Times(1);
  EXPECT_CALL(observer(), OnPageActionChipShown(testing::_)).Times(0);
  model.NotifyChanged();

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipShown(testing::_)).Times(0);
  EXPECT_CALL(observer(), OnPageActionChipHidden(testing::_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest,
       OnPageActionChipVisibilityConsidersIconVisibility) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(false));
  ON_CALL(model, GetShowSuggestionChip()).WillByDefault(testing::Return(true));
  observer().RegisterAsPageActionObserver(controller());

  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  EXPECT_CALL(observer(), OnPageActionChipShown(testing::_)).Times(1);
  model.NotifyChanged();

  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(false));
  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionChipHidden(testing::_)).Times(1);
  model.NotifyChanged();
}

// Ensures that the initial state of a page action model is applied when
// registering as the observer. Further change notifications shouldn't
// have an effect.
TEST_F(PageActionObserverTest, PageActionInitialStateShown) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(true));
  observer().RegisterAsPageActionObserver(controller());

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(testing::_)).Times(0);
  model.NotifyChanged();
}

TEST_F(PageActionObserverTest, PageActionInitialStateHidden) {
  NotifierPageActionModel& model = factory().Get(kTestPageActionId);
  ON_CALL(model, GetVisible()).WillByDefault(testing::Return(false));
  observer().RegisterAsPageActionObserver(controller());

  // Further change notifications shouldn't trigger the event anymore.
  EXPECT_CALL(observer(), OnPageActionIconShown(testing::_)).Times(0);
  model.NotifyChanged();
}

}  // namespace
}  // namespace page_actions
