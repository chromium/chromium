// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

AutofillBubbleControllerBase::AutofillBubbleControllerBase(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

AutofillBubbleControllerBase::~AutofillBubbleControllerBase() {
  HideBubble();
}
void AutofillBubbleControllerBase::Show() {
  UpdatePageActionIcon();
  DoShowBubble();
  UpdatePageActionIcon();
}

void AutofillBubbleControllerBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HideBubble();
}

void AutofillBubbleControllerBase::WebContentsDestroyed() {
  HideBubble();
}

void AutofillBubbleControllerBase::UpdatePageActionIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    browser->window()->UpdatePageActionIcon(GetPageActionIconType());
}

void AutofillBubbleControllerBase::HideBubble() {
  if (bubble_view_) {
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

}  // namespace autofill
