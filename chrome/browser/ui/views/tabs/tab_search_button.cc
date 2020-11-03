// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/tab_search/tab_search_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/widget/widget.h"

namespace {

TabSearchOpenAction GetActionForEvent(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return TabSearchOpenAction::kMouseClick;
  }
  return event.IsKeyEvent() ? TabSearchOpenAction::kKeyboardNavigation
                            : TabSearchOpenAction::kTouchGesture;
}

}  // namespace

TabSearchButton::TabSearchButton(TabStrip* tab_strip)
    : NewTabButton(tab_strip, PressedCallback()) {
  SetImageHorizontalAlignment(HorizontalAlignment::ALIGN_CENTER);
  SetImageVerticalAlignment(VerticalAlignment::ALIGN_MIDDLE);

  auto menu_button_controller = std::make_unique<views::MenuButtonController>(
      this,
      base::BindRepeating(&TabSearchButton::ButtonPressed,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));
}

TabSearchButton::~TabSearchButton() = default;

void TabSearchButton::NotifyClick(const ui::Event& event) {
  // Run pressed callback via MenuButtonController, instead of directly.
  menu_button_controller_->Activate(&event);
}

void TabSearchButton::FrameColorsChanged() {
  NewTabButton::FrameColorsChanged();
  // Icon color needs to be updated here as this is called when the hosting
  // window switches between active and inactive states. In each state the
  // foreground color of the tab controls is expected to change.
  SetImage(Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kTabSearchIcon, GetForegroundColor()));
}

void TabSearchButton::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_, widget);
  observed_bubble_widget_.Remove(bubble_);
  bubble_ = nullptr;
  pressed_lock_.reset();
  tab_strip()->OnTabSearchBubbleClosed();
}

bool TabSearchButton::ShowTabSearchBubble() {
  if (bubble_)
    return false;
  bubble_ = TabSearchBubbleView::CreateTabSearchBubble(
      tab_strip()->controller()->GetProfile(), this);
  observed_bubble_widget_.Add(bubble_);

  // Hold the pressed lock while the |bubble_| is active.
  pressed_lock_ = menu_button_controller_->TakeLock();
  return true;
}

bool TabSearchButton::IsBubbleVisible() const {
  return bubble_ && bubble_->IsVisible();
}

void TabSearchButton::PaintIcon(gfx::Canvas* canvas) {
  // Call ImageButton::PaintButtonContents() to paint the TabSearchButton's
  // VectorIcon.
  views::ImageButton::PaintButtonContents(canvas);
}

void TabSearchButton::ButtonPressed(const ui::Event& event) {
  // Only log the open action if it resulted in creating a new instance of the
  // Tab Search bubble.
  if (ShowTabSearchBubble()) {
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  GetActionForEvent(event));
  }
}
