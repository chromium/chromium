// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_bubble.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "content/public/browser/web_contents.h"

PermissionPromptBubble::PermissionPromptBubble(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : PermissionPromptDesktop(browser, web_contents, delegate),
      permission_requested_time_(base::TimeTicks::Now()) {
  ShowBubble();
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

void PermissionPromptBubble::UpdateAnchor() {
  bool was_browser_changed = UpdateBrowser();
  LocationBarView* lbv = GetLocationBarView();
  DCHECK(!lbv->IsChipActive());
  // TODO(crbug.com/1175231): Investigate why prompt_bubble_ can be null
  // here. Early return is preventing the crash from happening but we still
  // don't know the reason why it is null here and cannot reproduce it.
  if (!prompt_bubble_)
    return;

  // If |browser_| changed, recreate bubble for correct browser.
  if (was_browser_changed) {
    CleanUpPromptBubble();
    ShowBubble();
  } else {
    prompt_bubble_->UpdateAnchorPosition();
  }
}

permissions::PermissionPromptDisposition
PermissionPromptBubble::GetPromptDisposition() const {
  return permissions::PermissionPromptDisposition::ANCHORED_BUBBLE;
}

views::Widget* PermissionPromptBubble::GetPromptBubbleWidgetForTesting() {
  return prompt_bubble_ ? prompt_bubble_->GetWidget() : nullptr;
}
