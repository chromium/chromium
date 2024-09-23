// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_base_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_bubble_view_factory.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/permissions/features.h"
#include "content/public/browser/web_contents.h"

PermissionPromptBubble::PermissionPromptBubble(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      permission_requested_time_(base::TimeTicks::Now()) {
  LocationBarView* lbv = GetLocationBarView();
  if (lbv && lbv->IsDrawn() &&
      delegate->Requests()[0]->IsConfirmationChipSupported()) {
    lbv->GetChipController()->InitializePermissionPrompt(
        delegate->GetWeakPtr(),
        base::BindOnce(&PermissionPromptBubble::ShowBubble,
                       weak_factory_.GetWeakPtr()));
  } else {
    ShowBubble();
  }
}

PermissionPromptBubble::~PermissionPromptBubble() {
  CleanUpPromptBubble();
  CHECK(!IsInObserverList());
}

void PermissionPromptBubble::ShowBubble() {
  raw_ptr<PermissionPromptBubbleBaseView> prompt_bubble =
      CreatePermissionPromptBubbleView(browser(), delegate()->GetWeakPtr(),
                                       permission_requested_time_,
                                       PermissionPromptStyle::kBubbleOnly);
  prompt_bubble_tracker_.SetView(prompt_bubble);
  prompt_bubble->Show();
  prompt_bubble->GetWidget()->AddObserver(this);
  parent_was_visible_when_activation_changed_ =
      prompt_bubble->GetWidget()->GetPrimaryWindowWidget()->IsVisible();

  disallowed_custom_cursors_scope_ =
      delegate()->GetAssociatedWebContents()->CreateDisallowCustomCursorScope(
          /*max_dimension_dips=*/0);
}

void PermissionPromptBubble::CleanUpPromptBubble() {
  if (GetPromptBubble()) {
    views::Widget* widget = GetPromptBubble()->GetWidget();
    widget->RemoveObserver(this);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    prompt_bubble_tracker_.SetView(nullptr);
    disallowed_custom_cursors_scope_.RunAndReset();
  }
}

void PermissionPromptBubble::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  prompt_bubble_tracker_.SetView(nullptr);
}

void PermissionPromptBubble::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  // This logic prevents clickjacking. See https://crbug.com/1160485
  if (active && !parent_was_visible_when_activation_changed_) {
    // If the widget is active and the primary window wasn't active the last
    // time activation changed, we know that the window just came to the
    // foreground and trigger input protection.
    GetPromptBubble()->AsDialogDelegate()->TriggerInputProtection(
        /*force_early=*/true);
  }
  parent_was_visible_when_activation_changed_ =
      GetPromptBubble()->GetWidget()->GetPrimaryWindowWidget()->IsVisible();
}

std::optional<gfx::Rect> PermissionPromptBubble::GetViewBoundsInScreen() const {
  return GetPromptBubble()
             ? std::make_optional<gfx::Rect>(
                   GetPromptBubble()->GetWidget()->GetWindowBoundsInScreen())
             : std::nullopt;
}

bool PermissionPromptBubble::UpdateAnchor() {
  bool was_browser_changed = UpdateBrowser();
  // TODO(crbug.com/40747230): Investigate why prompt_bubble_ can be null
  // here. Early return is preventing the crash from happening but we still
  // don't know the reason why it is null here and cannot reproduce it.
  if (!GetPromptBubble()) {
    return true;
  }

  // If |browser_| changed, we need to recreate bubble for correct browser.
  if (was_browser_changed) {
    CleanUpPromptBubble();
    return false;
  } else {
    GetPromptBubble()->UpdateAnchorPosition();
  }

  if (!delegate()->Requests().empty() &&
      delegate()->Requests()[0]->IsConfirmationChipSupported()) {
    // If we have a location bar view but the chip_controller_ doesn't exist,
    // it means that the we switched from a browser mode that did not have a
    // location bar view. In that case we should create the chip in the location
    // bar view if required, then obtain a reference to the chip controller and
    // finally initialize it with the current permission request.
    LocationBarView* lbv = GetLocationBarView();

    if (lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen() &&
        !lbv->IsEditingOrEmpty()) {
      auto* chip_controller = lbv->GetChipController();
      chip_controller->InitializePermissionPrompt(delegate()->GetWeakPtr());
    }
  }

  return true;
}

permissions::PermissionPromptDisposition
PermissionPromptBubble::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ANCHORED_BUBBLE;
}

views::Widget* PermissionPromptBubble::GetPromptBubbleWidgetForTesting() {
  return GetPromptBubble() ? GetPromptBubble()->GetWidget() : nullptr;
}

PermissionPromptBubbleBaseView* PermissionPromptBubble::GetPromptBubble() {
  return static_cast<PermissionPromptBubbleBaseView*>(
      prompt_bubble_tracker_.view());
}

const PermissionPromptBubbleBaseView* PermissionPromptBubble::GetPromptBubble()
    const {
  return static_cast<const PermissionPromptBubbleBaseView*>(
      prompt_bubble_tracker_.view());
}
