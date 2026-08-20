// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget.h"

namespace shared {
namespace {

class NewTabButtonTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.bounds = gfx::Rect(0, 0, 100, 100);
    widget_params.context = GetContext();
    widget_->Init(std::move(widget_params));

    button_ = widget_->SetContentsView(std::make_unique<NewTabButton>(
        browser(), /*button_size=*/28, /*icon_size=*/16));
    button_->SetBounds(0, 0, 28, 28);
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<NewTabButton> button_;
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
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  const int initial_count = tab_strip_model->count();

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

  EXPECT_EQ(tab_strip_model->count(), initial_count + 1);
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
