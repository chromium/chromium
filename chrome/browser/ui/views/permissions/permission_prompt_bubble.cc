// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
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
      base::FeatureList::IsEnabled(permissions::features::kConfirmationChip) &&
      delegate->Requests()[0]->IsConfirmationChipSupported()) {
    lbv->chip_controller()->InitializePermissionPrompt(
        web_contents, delegate->GetWeakPtr(),
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
  prompt_bubble_ = new PermissionPromptBubbleView(
      browser(), delegate()->GetWeakPtr(), permission_requested_time_,
      PermissionPromptStyle::kBubbleOnly);
  prompt_bubble_->Show();
  prompt_bubble_->GetWidget()->AddObserver(this);
  parent_was_visible_when_activation_changed_ =
      prompt_bubble_->GetWidget()->GetPrimaryWindowWidget()->IsVisible();
}

void PermissionPromptBubble::CleanUpPromptBubble() {
  if (prompt_bubble_) {
    views::Widget* widget = prompt_bubble_->GetWidget();
    widget->RemoveObserver(this);
    widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    prompt_bubble_ = nullptr;
  }
}

void PermissionPromptBubble::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
  prompt_bubble_ = nullptr;
}

void PermissionPromptBubble::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  // This logic prevents clickjacking. See https://crbug.com/1160485
  if (active && !parent_was_visible_when_activation_changed_) {
    // If the widget is active and the primary window wasn't active the last
    // time activation changed, we know that the window just came to the
    // foreground and trigger input protection.
    prompt_bubble_->AsDialogDelegate()->TriggerInputProtection();
  }
  parent_was_visible_when_activation_changed_ =
      prompt_bubble_->GetWidget()->GetPrimaryWindowWidget()->IsVisible();
}

bool PermissionPromptBubble::UpdateAnchor() {
  bool was_browser_changed = UpdateBrowser();
  // TODO(crbug.com/1175231): Investigate why prompt_bubble_ can be null
  // here. Early return is preventing the crash from happening but we still
  // don't know the reason why it is null here and cannot reproduce it.
  if (!prompt_bubble_)
    return true;

  // If |browser_| changed, we need to recreate bubble for correct browser.
  if (was_browser_changed) {
    CleanUpPromptBubble();
    return false;
  } else {
    prompt_bubble_->UpdateAnchorPosition();
  }

  if (base::FeatureList::IsEnabled(permissions::features::kConfirmationChip) &&
      !delegate()->Requests().empty() &&
      delegate()->Requests()[0]->IsConfirmationChipSupported()) {
    // If we have a location bar view but the chip_controller_ doesn't exist,
    // it means that the we switched from a browser mode that did not have a
    // location bar view. In that case we should create the chip in the location
    // bar view if required, then obtain a reference to the chip controller and
    // finally initialize it with the current permission request.
    LocationBarView* lbv = GetLocationBarView();

    if (lbv && lbv->IsDrawn() && !lbv->GetWidget()->IsFullscreen() &&
        !lbv->IsEditingOrEmpty()) {
      auto* chip_controller = lbv->chip_controller();
      chip_controller->InitializePermissionPrompt(
          web_contents(), delegate()->GetWeakPtr(), base::DoNothing());
    }
  }

  return true;
}

permissions::PermissionPromptDisposition
PermissionPromptBubble::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ANCHORED_BUBBLE;
}

views::Widget* PermissionPromptBubble::GetPromptBubbleWidgetForTesting() {
  return prompt_bubble_ ? prompt_bubble_->GetWidget() : nullptr;
}
