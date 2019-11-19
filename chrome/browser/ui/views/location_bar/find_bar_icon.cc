// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/ink_drop.h"

FindBarIcon::FindBarIcon(Browser* browser,
                         PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate), browser_(browser) {
  DCHECK(browser_);
}

FindBarIcon::~FindBarIcon() {}

void FindBarIcon::SetActive(bool activate, bool should_animate) {
  if (activate ==
      (GetInkDrop()->GetTargetInkDropState() == views::InkDropState::ACTIVATED))
    return;
  if (activate) {
    if (should_animate) {
      AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
    } else {
      GetInkDrop()->SnapToActivated();
    }
  } else {
    AnimateInkDrop(views::InkDropState::HIDDEN, nullptr);
  }
}

base::string16 FindBarIcon::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_FIND);
}

void FindBarIcon::OnExecuting(ExecuteSource execute_source) {}

views::BubbleDialogDelegateView* FindBarIcon::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& FindBarIcon::GetVectorIcon() const {
  return omnibox::kFindInPageIcon;
}

bool FindBarIcon::Update() {
  // |browser_->window()| may return nullptr because Update() is called while
  // BrowserWindow is being constructed.
  if (!browser_->window() || !browser_->HasFindBarController())
    return false;

  const bool was_visible = GetVisible();
  SetVisible(browser_->GetFindBarController()->find_bar()->IsFindBarVisible());
  const bool visibility_changed = was_visible != GetVisible();
  SetActive(GetVisible(), visibility_changed);
  return visibility_changed;
}
