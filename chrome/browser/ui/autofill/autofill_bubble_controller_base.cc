// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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
// Page action icons do not exist for Android.
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser)
    browser->window()->UpdatePageActionIcon(GetPageActionIconType());
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AutofillBubbleControllerBase::HideBubble() {
  if (bubble_view_) {
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

}  // namespace autofill
