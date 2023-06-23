// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

CookieControlsBubbleCoordinator::CookieControlsBubbleCoordinator(
    views::View* anchor_view)
    : anchor_view_(anchor_view) {}

CookieControlsBubbleCoordinator::~CookieControlsBubbleCoordinator() = default;

void CookieControlsBubbleCoordinator::ShowBubble(
    views::Button* highlighted_button,
    content::WebContents* web_contents,
    content_settings::CookieControlsController* controller) {
  if (bubble_view_ != nullptr) {
    return;
  }
  auto bubble_view =
      std::make_unique<CookieControlsBubbleView>(anchor_view_, web_contents);
  bubble_view_ = bubble_view.get();
  bubble_view_->View::AddObserver(this);

  view_controller_ = std::make_unique<CookieControlsBubbleViewController>(
      bubble_view_, controller);

  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  widget->Show();
}

CookieControlsBubbleView* CookieControlsBubbleCoordinator::GetBubble() {
  return bubble_view_;
}

void CookieControlsBubbleCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  bubble_view_ = nullptr;
  view_controller_ = nullptr;
}
