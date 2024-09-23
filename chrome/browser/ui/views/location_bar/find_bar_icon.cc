// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/find_bar_icon.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"

FindBarIcon::FindBarIcon(
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "Find"),
      browser_(browser) {
  DCHECK(browser_);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(IDS_TOOLTIP_FIND));
}

FindBarIcon::~FindBarIcon() {}

void FindBarIcon::SetActive(bool activate, bool should_animate) {
  if (activate ==
      (views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() ==
       views::InkDropState::ACTIVATED))
    return;
  if (activate) {
    if (should_animate) {
      views::InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTIVATED,
                                                nullptr);
    } else {
      views::InkDrop::Get(this)->GetInkDrop()->SnapToActivated();
    }
  } else {
    views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN,
                                              nullptr);
  }
}

void FindBarIcon::OnExecuting(ExecuteSource execute_source) {}

views::BubbleDialogDelegate* FindBarIcon::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& FindBarIcon::GetVectorIcon() const {
  return omnibox::kFindInPageChromeRefreshIcon;
}

void FindBarIcon::UpdateImpl() {
  // |browser_->window()| may return nullptr because Update() is called while
  // BrowserWindow is being constructed.
  if (!browser_->window() || !browser_->HasFindBarController())
    return;

  const bool was_visible = GetVisible();
  SetVisible(browser_->GetFindBarController()->find_bar()->IsFindBarVisible());
  SetActive(GetVisible(), was_visible != GetVisible());
}

BEGIN_METADATA(FindBarIcon)
END_METADATA
