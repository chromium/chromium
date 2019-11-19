// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls_icon_view.h"

#include <memory>
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"

CookieControlsIconView::CookieControlsIconView(
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(nullptr, 0, delegate) {
  SetVisible(false);
}

CookieControlsIconView::~CookieControlsIconView() = default;

bool CookieControlsIconView::Update() {
  bool was_visible = GetVisible();
  auto* web_contents = delegate()->GetWebContentsForPageActionIconView();
  if (web_contents) {
    if (!controller_) {
      controller_ = std::make_unique<CookieControlsController>(web_contents);
      observer_.Add(controller_.get());
    }
    controller_->Update(web_contents);
  }
  SetVisible(ShouldBeVisible());

  return GetVisible() != was_visible;
}

void CookieControlsIconView::OnStatusChanged(
    CookieControlsController::Status status,
    int blocked_cookies) {
  if (status_ != status) {
    status_ = status;
    SetVisible(ShouldBeVisible());
    UpdateIconImage();
  }
  OnBlockedCookiesCountChanged(blocked_cookies);
}

void CookieControlsIconView::OnBlockedCookiesCountChanged(int blocked_cookies) {
  // The blocked cookie count changes quite frequently, so avoid unnecessary
  // UI updates.
  if (has_blocked_cookies_ != blocked_cookies > 0) {
    has_blocked_cookies_ = blocked_cookies > 0;
    SetVisible(ShouldBeVisible());
  }
}

bool CookieControlsIconView::ShouldBeVisible() const {
  if (delegate()->IsLocationBarUserInputInProgress())
    return false;

  if (HasAssociatedBubble())
    return true;

  if (!delegate()->GetWebContentsForPageActionIconView())
    return false;

  switch (status_) {
    case CookieControlsController::Status::kDisabledForSite:
      return true;
    case CookieControlsController::Status::kEnabled:
      return has_blocked_cookies_;
    case CookieControlsController::Status::kDisabled:
    case CookieControlsController::Status::kUninitialized:
      return false;
  }
}

bool CookieControlsIconView::HasAssociatedBubble() const {
  if (!GetBubble())
    return false;

  // There may be multiple icons but only a single bubble can be displayed
  // at a time. Check if the bubble belongs to this icon.
  if (!GetBubble()->GetAnchorView())
    return false;
  return GetBubble()->GetAnchorView()->GetWidget() == GetWidget();
}

void CookieControlsIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  CookieControlsBubbleView::ShowBubble(
      this, this, delegate()->GetWebContentsForPageActionIconView(),
      controller_.get(), status_);
}

views::BubbleDialogDelegateView* CookieControlsIconView::GetBubble() const {
  return CookieControlsBubbleView::GetCookieBubble();
}

const gfx::VectorIcon& CookieControlsIconView::GetVectorIcon() const {
  if (status_ == CookieControlsController::Status::kDisabledForSite)
    return kEyeIcon;
  return kEyeCrossedIcon;
}

base::string16 CookieControlsIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_COOKIE_CONTROLS_TOOLTIP);
}
