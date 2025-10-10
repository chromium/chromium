// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/bubble_manager.h"
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
  if (IsShowingBubble()) {
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

void AutofillBubbleControllerBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (IsBubbleManagerEnabled()) {
    // BubbleManager will handle the effects of tab changes.
    return;
  }

  if (visibility == content::Visibility::HIDDEN) {
    HideBubble(/*initiated_by_bubble_manager=*/false);
  }
}

void AutofillBubbleControllerBase::WebContentsDestroyed() {
  if (IsShowingBubble()) {
    bubble_view_->Hide();
    bubble_view_ = nullptr;
  }
}

void AutofillBubbleControllerBase::UpdatePageActionIcon() {
  // Page action icons do not exist for Android.
#if !BUILDFLAG(IS_ANDROID)
  if (auto icon_type = GetPageActionIconType()) {
    if (Browser* browser = chrome::FindBrowserWithTab(web_contents())) {
      browser->window()->UpdatePageActionIcon(*icon_type);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AutofillBubbleControllerBase::ShowBubble() {
  was_bubble_shown_ = true;
  UpdatePageActionIcon();
  DoShowBubble();
  UpdatePageActionIcon();
}

void AutofillBubbleControllerBase::HideBubble(
    bool initiated_by_bubble_manager) {
  if (IsShowingBubble()) {
    bubble_hide_initiated_by_bubble_manager_ = initiated_by_bubble_manager;
    bubble_view_->Hide();
    ResetBubbleViewAndInformBubbleManager();
  }
  bubble_hide_initiated_by_bubble_manager_ = false;
}

bool AutofillBubbleControllerBase::CanBeReshown() const {
  return true;
}

bool AutofillBubbleControllerBase::IsShowingBubble() const {
  return bubble_view_ != nullptr;
}

bool AutofillBubbleControllerBase::IsMouseHovered() const {
  return IsShowingBubble() && bubble_view_->IsMouseHovered();
}

bool AutofillBubbleControllerBase::MaySetUpBubble() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else  // BUILDFLAG(IS_ANDROID)
  if (!IsBubbleManagerEnabled()) {
    return true;
  }

  auto* manager = BubbleManager::GetForWebContents(web_contents());
  return manager && !manager->HasPendingBubbleOfSameType(GetBubbleType());
#endif
}

void AutofillBubbleControllerBase::QueueOrShowBubble(bool force_show) {
#if !BUILDFLAG(IS_ANDROID)
  if (IsBubbleManagerEnabled()) {
    if (auto* manager = BubbleManager::GetForWebContents(web_contents())) {
      manager->RequestShowController(*this, force_show);
    }
    return;
  }
#endif

  ShowBubble();
}

void AutofillBubbleControllerBase::SetBubbleView(
    AutofillBubbleBase& bubble_view) {
  bubble_view_ = &bubble_view;
}

void AutofillBubbleControllerBase::ResetBubbleViewAndInformBubbleManager() {
#if !BUILDFLAG(IS_ANDROID)
  const bool was_showing = IsShowingBubble();
#endif  // !BUILDFLAG(IS_ANDROID)

  bubble_view_ = nullptr;

#if !BUILDFLAG(IS_ANDROID)
  if (was_showing && base::FeatureList::IsEnabled(
                         features::kAutofillShowBubblesBasedOnPriorities)) {
    if (auto* manager = BubbleManager::GetForWebContents(web_contents())) {
      manager->OnBubbleHiddenByController(*this,
                                          allow_bubble_manager_to_show_next_);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace autofill
