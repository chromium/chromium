// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace shared {
namespace {

class NewTabButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
    ON_CALL(testing::Const(*mock_browser_window_interface_), GetProfile())
        .WillByDefault(testing::Return(profile_.get()));

    actions::ActionManager::Get().ResetActions();
    auto root = actions::ActionItem::Builder().Build();
    root->AddChild(actions::ActionItem::Builder(
                       base::BindRepeating(
                           [](bool* action_invoked, actions::ActionItem* item,
                              actions::ActionInvocationContext context) {
                             *action_invoked = true;
                           },
                           &new_tab_action_invoked_))
                       .SetActionId(kActionNewTab)
                       .SetText(u"New Tab")
                       .Build());
    actions::ActionItem* root_action =
        actions::ActionManager::Get().AddAction(std::move(root));

    browser_actions_ =
        std::make_unique<BrowserActions>(mock_browser_window_interface_.get());
    browser_actions_->set_root_action_item_for_testing(root_action);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_params.context = GetContext();
    widget_->Init(std::move(widget_params));

    button_ = widget_->SetContentsView(
        std::make_unique<NewTabButton>(mock_browser_window_interface_.get(),
                                       /*button_size=*/28, /*icon_size=*/16));
    button_->SetBounds(0, 0, 28, 28);
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    browser_actions_.reset();
    actions::ActionManager::Get().ResetActions();
    mock_browser_window_interface_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
  std::unique_ptr<BrowserActions> browser_actions_;
  bool new_tab_action_invoked_ = false;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<NewTabButton> button_ = nullptr;
};

TEST_F(NewTabButtonTest, TriggerableEventFlags) {
#if BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(button_->GetTriggerableEventFlags() & ui::EF_MIDDLE_MOUSE_BUTTON);
#else
  EXPECT_FALSE(button_->GetTriggerableEventFlags() &
               ui::EF_MIDDLE_MOUSE_BUTTON);
#endif
  EXPECT_TRUE(button_->GetTriggerableEventFlags() & ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(NewTabButtonTest, LeftClickButtonCreatesNewTabAndUpdatesInkDrop) {
  EXPECT_FALSE(new_tab_action_invoked_);

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button_->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_TRUE(new_tab_action_invoked_);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}

#if BUILDFLAG(IS_LINUX)
TEST_F(NewTabButtonTest, MiddleClickExecutesCallbackAndUpdatesInkDropOnLinux) {
  bool callback_called = false;
  button_->SetMiddleClickCallbackForTesting(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON);
  EXPECT_TRUE(button_->OnMousePressed(press_event));
  EXPECT_EQ(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_MIDDLE_MOUSE_BUTTON,
                               ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_TRUE(callback_called);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}
#else
TEST_F(NewTabButtonTest, MiddleClickIgnoredOnNonLinux) {
  bool callback_called = false;
  button_->SetMiddleClickCallbackForTesting(base::BindRepeating(
      [](bool* called) { *called = true; }, &callback_called));

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), base::TimeTicks::Now(),
                             ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMousePressed(press_event);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);

  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), base::TimeTicks::Now(),
                               ui::EF_MIDDLE_MOUSE_BUTTON,
                               ui::EF_MIDDLE_MOUSE_BUTTON);
  button_->OnMouseReleased(release_event);

  EXPECT_FALSE(callback_called);
  EXPECT_NE(views::InkDrop::Get(button_)->GetInkDrop()->GetTargetInkDropState(),
            views::InkDropState::ACTION_PENDING);
}
#endif

}  // namespace
}  // namespace shared
