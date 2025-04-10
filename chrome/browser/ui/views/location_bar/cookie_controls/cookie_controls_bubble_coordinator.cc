// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include "chrome/browser/ui/page_action/page_action_icon_type.h"
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
    ToolbarButtonProvider* toolbar_button_provider,
    content::WebContents* web_contents,
    content_settings::CookieControlsController* controller) {
  CHECK(toolbar_button_provider);
  if (bubble_view_ != nullptr) {
    return;
  }

  // TODO(crbug.com/376283777): An action ID should be created and used here
  // when Cookie Controls is migrated to the new page actions framework.
  views::View* anchor_view =
      toolbar_button_provider->GetAnchorView(std::nullopt);
  auto bubble_view = std::make_unique<CookieControlsBubbleViewImpl>(
      anchor_view, web_contents,
      base::BindOnce(&CookieControlsBubbleCoordinator::OnViewIsDeleting,
                     base::Unretained(this)));
  bubble_view_ = bubble_view.get();
  bubble_view_->View::AddObserver(this);

  auto* icon_view = toolbar_button_provider->GetPageActionIconView(
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
  view_controller_->SetIsReloadingState(false);
  controller->Update(web_contents);
  widget->Show();
}

bool CookieControlsBubbleCoordinator::IsReloadingState() const {
  if (!view_controller_) {
    return false;
  }
  return view_controller_->IsReloadingState();
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
