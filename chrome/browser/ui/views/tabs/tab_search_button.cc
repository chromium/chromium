// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_button.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/widget/widget.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabSearchOpenAction {
  kMouseClick = 0,
  kKeyboardNavigation = 1,
  kKeyboardShortcut = 2,
  kTouchGesture = 3,
  kMaxValue = kTouchGesture,
};

TabSearchOpenAction GetActionForEvent(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return TabSearchOpenAction::kMouseClick;
  }
  return event.IsKeyEvent() ? TabSearchOpenAction::kKeyboardNavigation
                            : TabSearchOpenAction::kTouchGesture;
}

}  // namespace

TabSearchButton::TabSearchButton(TabStrip* tab_strip)
    : NewTabButton(tab_strip, PressedCallback()),
      webui_bubble_manager_(this,
                            tab_strip->controller()->GetProfile(),
                            GURL(chrome::kChromeUITabSearchURL),
                            IDS_ACCNAME_TAB_SEARCH,
                            true),
      widget_open_timer_(base::BindRepeating([](base::TimeDelta time_elapsed) {
        base::UmaHistogramMediumTimes("Tabs.TabSearch.WindowDisplayedDuration3",
                                      time_elapsed);
      })) {
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

void TabSearchButton::OnWidgetVisibilityChanged(views::Widget* widget,
                                                bool visible) {
  DCHECK_EQ(webui_bubble_manager_.GetBubbleWidget(), widget);
  if (visible && bubble_created_time_.has_value()) {
    GetWidget()->GetCompositor()->RequestPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::TimeTicks bubble_created_time,
               bool bubble_using_cached_web_contents,
               const gfx::PresentationFeedback& feedback) {
              base::UmaHistogramMediumTimes(
                  bubble_using_cached_web_contents
                      ? "Tabs.TabSearch.WindowTimeToShowCachedWebView"
                      : "Tabs.TabSearch.WindowTimeToShowUncachedWebView",
                  feedback.timestamp - bubble_created_time);
            },
            *bubble_created_time_,
            webui_bubble_manager_.bubble_using_cached_web_contents()));
    bubble_created_time_.reset();
  }
}

void TabSearchButton::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(webui_bubble_manager_.GetBubbleWidget(), widget);
  DCHECK(bubble_widget_observation_.IsObservingSource(
      webui_bubble_manager_.GetBubbleWidget()));
  bubble_widget_observation_.Reset();
  pressed_lock_.reset();
}

bool TabSearchButton::ShowTabSearchBubble(bool triggered_by_keyboard_shortcut) {
  if (webui_bubble_manager_.GetBubbleWidget())
    return false;

  bubble_created_time_ = base::TimeTicks::Now();
  webui_bubble_manager_.ShowBubble();

  if (triggered_by_keyboard_shortcut) {
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  TabSearchOpenAction::kKeyboardShortcut);
  }

  // There should only ever be a single bubble widget active for the
  // TabSearchButton.
  DCHECK(!bubble_widget_observation_.IsObserving());
  bubble_widget_observation_.Observe(webui_bubble_manager_.GetBubbleWidget());
  widget_open_timer_.Reset(webui_bubble_manager_.GetBubbleWidget());

  // Hold the pressed lock while the |bubble_| is active.
  pressed_lock_ = menu_button_controller_->TakeLock();
  return true;
}

void TabSearchButton::CloseTabSearchBubble() {
  webui_bubble_manager_.CloseBubble();
}

void TabSearchButton::PaintIcon(gfx::Canvas* canvas) {
  // Call ImageButton::PaintButtonContents() to paint the TabSearchButton's
  // VectorIcon.
  views::ImageButton::PaintButtonContents(canvas);
}

void TabSearchButton::ButtonPressed(const ui::Event& event) {
  if (ShowTabSearchBubble()) {
    // Only log the open action if it resulted in creating a new instance of the
    // Tab Search bubble.
    base::UmaHistogramEnumeration("Tabs.TabSearch.OpenAction",
                                  GetActionForEvent(event));
    return;
  }
  CloseTabSearchBubble();
}

BEGIN_METADATA(TabSearchButton, NewTabButton)
END_METADATA
