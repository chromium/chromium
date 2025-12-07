// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include "base/callback_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_impl.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "ui/actions/actions.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

namespace {
actions::ActionItem* GetActionItem(actions::ActionItem* root_action_item) {
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionShowCookieControls, root_action_item);
  CHECK(action_item);
  return action_item;
}
}  // namespace

DEFINE_USER_DATA(CookieControlsBubbleCoordinator);

CookieControlsBubbleCoordinator::CookieControlsBubbleCoordinator(
    BrowserWindowInterface* browser_window,
    actions::ActionItem* root_action_item)
    : scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this),
      action_item_(GetActionItem(root_action_item)) {}

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

  auto* icon_view =
      toolbar_button_provider->GetPageActionView(kActionShowCookieControls);
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

  action_item_->SetIsShowingBubble(true);
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

base::CallbackListSubscription
CookieControlsBubbleCoordinator::RegisterBubbleClosingCallback(
    base::RepeatingClosure callback) {
  return bubble_closing_callbacks_.Add(std::move(callback));
}

void CookieControlsBubbleCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  bubble_view_ = nullptr;
  view_controller_ = nullptr;
  bubble_closing_callbacks_.Notify();

  action_item_->SetIsShowingBubble(false);
}

// static
CookieControlsBubbleCoordinator* CookieControlsBubbleCoordinator::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}
