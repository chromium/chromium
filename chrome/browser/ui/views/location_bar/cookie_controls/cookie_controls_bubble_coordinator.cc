// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

namespace {}  // namespace

CookieControlsBubbleCoordinator::CookieControlsBubbleCoordinator() = default;

CookieControlsBubbleCoordinator::~CookieControlsBubbleCoordinator() = default;

void CookieControlsBubbleCoordinator::ShowBubble(
    content::WebContents* web_contents,
    content_settings::CookieControlsController* controller) {
  if (bubble_view_ != nullptr) {
    return;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAnchorView(
          PageActionIconType::kCookieControls);

  auto bubble_view = std::make_unique<CookieControlsBubbleViewImpl>(
      anchor_view, web_contents,
      base::BindOnce(&CookieControlsBubbleCoordinator::OnViewIsDeleting,
                     base::Unretained(this)));
  bubble_view_ = bubble_view.get();
  bubble_view_->View::AddObserver(this);

  auto* icon_view =
      browser_view->toolbar_button_provider()->GetPageActionIconView(
          PageActionIconType::kCookieControls);
  CHECK(icon_view);
  bubble_view_->SetHighlightedButton(icon_view);

  view_controller_ = std::make_unique<CookieControlsBubbleViewController>(
      bubble_view_, controller, web_contents);
  if (display_name_for_testing_.has_value()) {
    view_controller_->SetSubjectUrlNameForTesting(
        display_name_for_testing_.value());
  }

  views::Widget* const widget =
      views::BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  controller->Update(web_contents);
  widget->Show();
}

CookieControlsBubbleViewImpl* CookieControlsBubbleCoordinator::GetBubble()
    const {
  return bubble_view_;
}

CookieControlsBubbleViewController*
CookieControlsBubbleCoordinator::GetViewControllerForTesting() {
  return view_controller_.get();
}

void CookieControlsBubbleCoordinator::SetDisplayNameForTesting(
    const std::u16string& name) {
  display_name_for_testing_ = name;
  if (view_controller_ != nullptr) {
    view_controller_->SetSubjectUrlNameForTesting(
        display_name_for_testing_.value());
  }
}

void CookieControlsBubbleCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  bubble_view_ = nullptr;
  view_controller_ = nullptr;
}
