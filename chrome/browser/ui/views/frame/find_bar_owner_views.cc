// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/find_bar_owner_views.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

FindBarOwnerViews::FindBarOwnerViews(BrowserView* browser_view)
    : browser_view_(browser_view) {}

FindBarOwnerViews::~FindBarOwnerViews() = default;

views::Widget* FindBarOwnerViews::GetOwnerWidget() {
  return browser_view_->GetWidget();
}

gfx::Rect FindBarOwnerViews::GetFindBarBoundingBox() {
  return browser_view_->GetFindBarBoundingBox();
}

gfx::Rect FindBarOwnerViews::GetFindBarClippingBox() {
  return browser_view_->bounds();
}

bool FindBarOwnerViews::IsOffTheRecord() const {
  return browser_view_->browser()->profile()->IsOffTheRecord();
}

views::Widget* FindBarOwnerViews::GetWidgetForAnchoring() {
  return browser_view_->GetWidgetForAnchoring();
}

std::u16string FindBarOwnerViews::GetFindBarAccessibleWindowTitle() {
  return l10n_util::GetStringFUTF16(
      IDS_FIND_IN_PAGE_ACCESSIBLE_TITLE,
      browser_view_->browser()->GetWindowTitleForCurrentTab(false));
}

void FindBarOwnerViews::OnFindBarVisibilityChanged(gfx::Rect visible_bounds) {
  // Tell the immersive mode controller about the find bar's new bounds. The
  // immersive mode controller uses the bounds to keep the top-of-window views
  // revealed when the mouse is hovered over the find bar.
  ImmersiveModeController::From(browser_view_->browser())
      ->OnFindBarVisibleBoundsChanged(visible_bounds);
  browser_view_->browser()->OnFindBarVisibilityChanged();
}

void FindBarOwnerViews::CloseOverlappingBubbles() {
  if (TranslateBubbleController* controller =
      TranslateBubbleController::From(browser_view_->browser())) {
    controller->CloseBubble();
  }
}
