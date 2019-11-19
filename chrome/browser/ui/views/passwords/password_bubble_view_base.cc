// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "chrome/browser/ui/views/passwords/password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/password_items_view.h"
#include "chrome/browser/ui/views/passwords/password_pending_view.h"
#include "chrome/browser/ui/views/passwords/password_save_confirmation_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/views/controls/button/button.h"

// static
PasswordBubbleViewBase* PasswordBubbleViewBase::g_manage_passwords_bubble_ =
    nullptr;

// static
void PasswordBubbleViewBase::ShowBubble(content::WebContents* web_contents,
                                        DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(!g_manage_passwords_bubble_ ||
         !g_manage_passwords_bubble_->GetWidget()->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ToolbarButtonProvider* button_provider =
      browser_view->toolbar_button_provider();
  views::View* anchor_view =
      button_provider->GetAnchorView(PageActionIconType::kManagePasswords);

  PasswordBubbleViewBase* bubble =
      CreateBubble(web_contents, anchor_view, reason);
  DCHECK(bubble);
  DCHECK_EQ(bubble, g_manage_passwords_bubble_);

  g_manage_passwords_bubble_->SetHighlightedButton(
      button_provider->GetPageActionIconView(
          PageActionIconType::kManagePasswords));

  views::BubbleDialogDelegateView::CreateBubble(g_manage_passwords_bubble_);

  g_manage_passwords_bubble_->ShowForReason(reason);
}

// static
PasswordBubbleViewBase* PasswordBubbleViewBase::CreateBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason) {
  PasswordBubbleViewBase* view = nullptr;
  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(web_contents)->GetState();
  if (model_state == password_manager::ui::MANAGE_STATE) {
    view = new PasswordItemsView(web_contents, anchor_view, reason);
  } else if (model_state == password_manager::ui::AUTO_SIGNIN_STATE) {
    view = new PasswordAutoSignInView(web_contents, anchor_view, reason);
  } else if (model_state == password_manager::ui::CONFIRMATION_STATE) {
    view = new PasswordSaveConfirmationView(web_contents, anchor_view, reason);
  } else if (model_state ==
                 password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
             model_state == password_manager::ui::PENDING_PASSWORD_STATE) {
    view = new PasswordPendingView(web_contents, anchor_view, reason);
  } else {
    NOTREACHED();
  }

  g_manage_passwords_bubble_ = view;
  return view;
}

// static
void PasswordBubbleViewBase::CloseCurrentBubble() {
  if (g_manage_passwords_bubble_)
    g_manage_passwords_bubble_->GetWidget()->Close();
}

// static
void PasswordBubbleViewBase::ActivateBubble() {
  DCHECK(g_manage_passwords_bubble_);
  DCHECK(g_manage_passwords_bubble_->GetWidget()->IsVisible());
  g_manage_passwords_bubble_->GetWidget()->Activate();
}

const content::WebContents* PasswordBubbleViewBase::GetWebContents() const {
  return model_.GetWebContents();
}

base::string16 PasswordBubbleViewBase::GetWindowTitle() const {
  return model_.title();
}

bool PasswordBubbleViewBase::ShouldShowWindowTitle() const {
  return !model_.title().empty();
}

PasswordBubbleViewBase::PasswordBubbleViewBase(
    content::WebContents* web_contents,
    views::View* anchor_view,
    DisplayReason reason,
    bool easily_dismissable)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      model_(PasswordsModelDelegateFromWebContents(web_contents),
             reason == AUTOMATIC ? ManagePasswordsBubbleModel::AUTOMATIC
                                 : ManagePasswordsBubbleModel::USER_ACTION) {
  // The |mouse_handler| closes the bubble if a keyboard or mouse interactions
  // happens outside of the bubble. By this the bubble becomes
  // 'easily-dissmisable' and this behavior can be enforced by the
  // corresponding flag.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kStickyBubble) ||
      easily_dismissable) {
    mouse_handler_ =
        std::make_unique<WebContentMouseHandler>(this, web_contents);
  }
}

PasswordBubbleViewBase::~PasswordBubbleViewBase() {
  if (g_manage_passwords_bubble_ == this)
    g_manage_passwords_bubble_ = nullptr;
}

void PasswordBubbleViewBase::OnWidgetClosing(views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetClosing(widget);
  if (widget != GetWidget())
    return;
  mouse_handler_.reset();
  // It can be the case that a password bubble is being closed while another
  // password bubble is being opened. The metrics recorder can be shared between
  // them and it doesn't understand the sequence [open1, open2, close1, close2].
  // Therefore, we reset the model early (before the bubble destructor) to get
  // the following sequence of events [open1, close1, open2, close2].
  model_.OnBubbleClosing();
}
