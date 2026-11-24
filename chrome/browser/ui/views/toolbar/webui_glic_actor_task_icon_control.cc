// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_actor_task_icon_control.h"

#include <utility>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"
#include "chrome/browser/glic/public/service/legacy_glic_activity_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"

WebUIGlicActorTaskIconControl::WebUIGlicActorTaskIconControl(
    WebUIToolbarControlDelegate* delegate)
    : delegate_(delegate) {
  CHECK(delegate_);
}

WebUIGlicActorTaskIconControl::~WebUIGlicActorTaskIconControl() = default;

void WebUIGlicActorTaskIconControl::Init() {
  CHECK(!is_initialized_);
  is_initialized_ = true;
  UpdateState();
}

void WebUIGlicActorTaskIconControl::OnClicked() {
  if (!is_initialized_ || !ShouldShow()) {
    return;
  }
  BrowserWindowInterface* const browser = delegate_->GetBrowser();
  auto* activity_manager =
      glic::LegacyGlicActivityManager::Get(browser->GetProfile());
  if (!activity_manager) {
    return;
  }

  // Only show the bubble if the button is not currently pressed, matching
  // ToolbarView::OnGlicSecondaryButtonClicked().
  if (auto* controller =
          glic::GlicActivityListBubbleController::From(browser)) {
    if (!is_pressed_) {
      controller->ShowBubble();
    }
  }

  actor::ui::LogGlobalTaskIndicatorClick(
      activity_manager->GetCurrentActorTaskNudgeState());
}

void WebUIGlicActorTaskIconControl::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  // ShouldShow() also depends on `can_show_`, so changing `is_visible_` only
  // shows or hides the icon (and changes the toolbar's preferred size) when
  // `can_show_` is true. Compare before and after to notify only on a change.
  const bool was_showing = ShouldShow();
  is_visible_ = visible;
  if (!is_visible_) {
    is_showing_nudge_ = false;
    nudge_label_.clear();
    CancelPendingBubbleAnchorRequest();
  }
  UpdateState();
  if (was_showing != ShouldShow()) {
    delegate_->OnPreferredSizeChanged();
  }
}

void WebUIGlicActorTaskIconControl::SetCanShow(bool can_show) {
  if (can_show_ == can_show) {
    return;
  }
  // ShouldShow() also depends on `is_visible_`, so changing `can_show_` only
  // shows or hides the icon (and changes the toolbar's preferred size) when
  // `is_visible_` is true. Compare before and after to notify only on a change.
  const bool was_showing = ShouldShow();
  can_show_ = can_show;
  if (!can_show_) {
    CancelPendingBubbleAnchorRequest();
  }
  UpdateState();
  if (was_showing != ShouldShow()) {
    delegate_->OnPreferredSizeChanged();
  }
}

bool WebUIGlicActorTaskIconControl::IsVisible() const {
  return is_visible_;
}

void WebUIGlicActorTaskIconControl::ShowNudgeLabel(
    const std::u16string& nudge_label) {
  if (nudge_label_ == nudge_label) {
    return;
  }
  nudge_label_ = nudge_label;
  if (is_showing_nudge_) {
    UpdateState();
  }
}

void WebUIGlicActorTaskIconControl::SetTaskIconToDefault() {
  if (nudge_label_.empty() && !is_showing_nudge_) {
    return;
  }
  nudge_label_.clear();
  is_showing_nudge_ = false;
  UpdateState();
}

void WebUIGlicActorTaskIconControl::SetIsShowingNudge(bool is_showing) {
  const bool had_visible_nudge = GetIsShowingNudge();
  if (!is_showing) {
    nudge_label_.clear();
  } else if (!is_visible_) {
    return;
  }
  is_showing_nudge_ = is_showing;
  if (had_visible_nudge != GetIsShowingNudge()) {
    UpdateState();
  }
}

bool WebUIGlicActorTaskIconControl::GetIsShowingNudge() const {
  return is_showing_nudge_ && !nudge_label_.empty();
}

void WebUIGlicActorTaskIconControl::SetPressedState(bool is_pressed) {
  if (is_pressed_ == is_pressed) {
    return;
  }
  is_pressed_ = is_pressed;
  UpdateState();
}

void WebUIGlicActorTaskIconControl::GetBubbleAnchorAsync(
    base::OnceCallback<void(views::BubbleAnchor)> callback) {
  CancelPendingBubbleAnchorRequest();
  // A hidden icon never reports its element.
  if (!ShouldShow()) {
    return;
  }
  BrowserElements* const elements =
      BrowserElements::From(delegate_->GetBrowser());
  CHECK(elements);
  if (ui::TrackedElement* const element =
          elements->GetElement(kGlicActorTaskIconElementId)) {
    std::move(callback).Run(views::BubbleAnchor(element));
    return;
  }
  pending_bubble_anchor_callback_ = std::move(callback);
  anchor_element_shown_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementShownCallback(
          kGlicActorTaskIconElementId, elements->GetContext(),
          base::BindRepeating(
              &WebUIGlicActorTaskIconControl::OnAnchorElementShown,
              base::Unretained(this)));
}

void WebUIGlicActorTaskIconControl::OnAnchorElementShown(
    ui::TrackedElement* element) {
  anchor_element_shown_subscription_ = {};
  std::move(pending_bubble_anchor_callback_).Run(views::BubbleAnchor(element));
}

void WebUIGlicActorTaskIconControl::CancelPendingBubbleAnchorRequest() {
  anchor_element_shown_subscription_ = {};
  pending_bubble_anchor_callback_.Reset();
}

void WebUIGlicActorTaskIconControl::UpdateState() {
  if (!is_initialized_) {
    return;
  }
  auto state = toolbar_ui_api::mojom::GlicActorTaskIconState::New();
  state->should_show = ShouldShow();
  if (GetIsShowingNudge()) {
    std::string label = base::UTF16ToUTF8(nudge_label_);
    state->tooltip = label;
    state->nudge_label = std::move(label);
  } else {
    state->tooltip = l10n_util::GetStringUTF8(IDS_ACTOR_TASK_INDICATOR_TOOLTIP);
  }
  state->is_pressed = is_pressed_;
  delegate_->OnGlicActorTaskIconStateChanged(std::move(state));
}
