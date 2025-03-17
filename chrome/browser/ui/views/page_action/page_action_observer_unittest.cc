// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_observer.h"

#include "chrome/browser/ui/tabs/test/mock_tab_interface.h"
#include "chrome/browser/ui/views/page_action/mock_page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_actions {

static constexpr actions::ActionId kTestPageActionId = 0;

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

  void NotifyChanged() {
    observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                          *this);
  }

 private:
  base::ObserverList<PageActionModelObserver> observer_list_;
};

class FakePageActionModelFactory : public PageActionModelFactory {
 public:
  std::unique_ptr<PageActionModelInterface> Create(int action_id) override {
    auto model = std::make_unique<NotifierPageActionModel>();
    model_map_.emplace(action_id, model.get());
    return model;
  }

  // Model getter for tests to set expectations.
  NotifierPageActionModel& Get(int action_id) {
    auto id_to_model = model_map_.find(action_id);
    CHECK(id_to_model != model_map_.end());
    CHECK_NE(id_to_model->second, nullptr);
    return *id_to_model->second;
  }

 private:
  std::map<actions::ActionId, NotifierPageActionModel*> model_map_;
};

class PageActionObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    model_factory_ = std::make_unique<FakePageActionModelFactory>();
    controller_ =
        std::make_unique<PageActionController>(nullptr, model_factory_.get());
    controller_->Initialize(tab_, {kTestPageActionId});
  }

  void TearDown() override {
    controller_.reset();
    model_factory_.reset();
  }

  MockPageActionObserver& observer() { return observer_; }
  PageActionController& controller() { return *controller_; }
  FakePageActionModelFactory& factory() { return *model_factory_; }

 private:
  MockPageActionObserver observer_;
  tabs::MockTabInterface tab_;
  std::unique_ptr<FakePageActionModelFactory> model_factory_;
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

}  // namespace page_actions
