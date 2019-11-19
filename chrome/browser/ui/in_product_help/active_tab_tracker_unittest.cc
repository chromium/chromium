// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/in_product_help/active_tab_tracker.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/list_selection_model.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Mock;

namespace {

class TestTabStripModelDelegateNoUnloadListener
    : public TestTabStripModelDelegate {
 public:
  TestTabStripModelDelegateNoUnloadListener() = default;
  ~TestTabStripModelDelegateNoUnloadListener() override = default;

  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override {
    return false;
  }
};

constexpr base::TimeDelta kTimeStep = base::TimeDelta::FromSeconds(1);

}  // namespace

class ActiveTabTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
  }

  void AddTab(TabStripModel* model) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(profile_.get()));
    model->AppendWebContents(std::move(contents), true);
  }

  void CloseTabAt(TabStripModel* model, int index) {
    model->CloseWebContentsAt(index,
                              TabStripModel::CloseTypes::CLOSE_USER_GESTURE);
  }

  Profile* profile() { return profile_.get(); }
  base::SimpleTestTickClock* clock() { return &clock_; }

 private:
  // A |BrowserTaskEnvironment| is needed for creating and using
  // |WebContents|es in a unit test.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::SimpleTestTickClock clock_;
};

TEST_F(ActiveTabTrackerTest, NotifiesOnActiveTabClosed) {
  TestTabStripModelDelegateNoUnloadListener delegate;
  TabStripModel model(&delegate, profile());

  base::MockCallback<ActiveTabTracker::ActiveTabClosedCallback> cb;
  ActiveTabTracker tracker(clock(), cb.Get());
  tracker.AddTabStripModel(&model);

  AddTab(&model);
  AddTab(&model);
  clock()->Advance(kTimeStep);

  model.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  clock()->Advance(kTimeStep);

  EXPECT_CALL(cb, Run(&model, kTimeStep)).Times(1);
  CloseTabAt(&model, 0);

  tracker.RemoveTabStripModel(&model);

  model.DetachWebContentsAt(0);
}

TEST_F(ActiveTabTrackerTest, UpdatesTimes) {
  TestTabStripModelDelegateNoUnloadListener delegate;
  TabStripModel model(&delegate, profile());

  base::MockCallback<ActiveTabTracker::ActiveTabClosedCallback> cb;
  ActiveTabTracker tracker(clock(), cb.Get());
  tracker.AddTabStripModel(&model);

  AddTab(&model);
  AddTab(&model);
  model.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  clock()->Advance(kTimeStep);

  model.ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  model.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  EXPECT_CALL(cb, Run(&model, base::TimeDelta())).Times(1);
  CloseTabAt(&model, 0);

  tracker.RemoveTabStripModel(&model);

  model.DetachWebContentsAt(0);
}

TEST_F(ActiveTabTrackerTest, IgnoresInactiveTabs) {
  TestTabStripModelDelegateNoUnloadListener delegate;
  TabStripModel model(&delegate, profile());

  base::MockCallback<ActiveTabTracker::ActiveTabClosedCallback> cb;
  ActiveTabTracker tracker(clock(), cb.Get());
  tracker.AddTabStripModel(&model);

  AddTab(&model);
  AddTab(&model);
  model.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  EXPECT_CALL(cb, Run(_, _)).Times(0);
  CloseTabAt(&model, 1);

  tracker.RemoveTabStripModel(&model);

  model.DetachWebContentsAt(0);
}

TEST_F(ActiveTabTrackerTest, TracksMultipleTabStripModels) {
  TestTabStripModelDelegateNoUnloadListener delegate;
  TabStripModel model_1(&delegate, profile());
  TabStripModel model_2(&delegate, profile());

  base::MockCallback<ActiveTabTracker::ActiveTabClosedCallback> cb;
  ActiveTabTracker tracker(clock(), cb.Get());
  tracker.AddTabStripModel(&model_1);
  tracker.AddTabStripModel(&model_2);

  AddTab(&model_1);
  AddTab(&model_1);
  AddTab(&model_2);
  AddTab(&model_2);

  clock()->Advance(kTimeStep);
  model_1.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  clock()->Advance(kTimeStep);
  model_2.ActivateTabAt(0, {TabStripModel::GestureType::kOther});

  {
    InSequence seq;
    EXPECT_CALL(cb, Run(&model_1, kTimeStep)).Times(1);
    EXPECT_CALL(cb, Run(&model_2, base::TimeDelta())).Times(1);
  }

  CloseTabAt(&model_1, 0);
  CloseTabAt(&model_2, 0);

  tracker.RemoveTabStripModel(&model_1);
  tracker.RemoveTabStripModel(&model_2);

  model_1.DetachWebContentsAt(0);
  model_2.DetachWebContentsAt(0);
}

TEST_F(ActiveTabTrackerTest, StopsObservingUponRemove) {
  TestTabStripModelDelegateNoUnloadListener delegate;
  TabStripModel model(&delegate, profile());

  base::MockCallback<ActiveTabTracker::ActiveTabClosedCallback> cb;
  ActiveTabTracker tracker(clock(), cb.Get());
  tracker.AddTabStripModel(&model);

  AddTab(&model);
  AddTab(&model);

  tracker.RemoveTabStripModel(&model);

  EXPECT_CALL(cb, Run(_, _)).Times(0);
  CloseTabAt(&model, 0);

  model.DetachWebContentsAt(0);
}
