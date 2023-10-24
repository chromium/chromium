// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
// This is not the real height of the bubble, it is used only to initialize the
// view and the real height is sent by DiceWebSigninInterceptHandler and set on
// SetHeightAndShowWidget().
constexpr int kInterceptionBubbleBaseHeight = 500;
constexpr int kInterceptionBubbleWidth = 290;

views::View* GetBubbleAnchorView(const Browser& browser) {
  return BrowserView::GetBrowserViewForBrowser(&browser)
      ->toolbar_button_provider()
      ->GetAvatarToolbarButton();
}

}  // namespace

DiceWebSigninInterceptionBubbleView::~DiceWebSigninInterceptionBubbleView() {
  // Cancel if the bubble is destroyed without user interaction.
  if (callback_) {
    RecordInterceptionResult(bubble_parameters_, profile_,
                             SigninInterceptionResult::kIgnored);
    // The callback may synchronously delete a handle, which would attempt to
    // close this bubble while it is being destroyed. Invalidate the handles now
    // to prevent this.
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(SigninInterceptionResult::kIgnored);
  }
}

// static
std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptionBubbleView::CreateBubble(
    Browser* browser,
    views::View* anchor_view,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  auto interception_bubble =
      base::WrapUnique(new DiceWebSigninInterceptionBubbleView(
          browser, anchor_view, bubble_parameters, std::move(callback)));
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> handle =
      interception_bubble->GetHandle();
  // The widget is owned by the views system and shown after the view is loaded
  // and the final height of the bubble is sent from
  // DiceWebSigninInterceptHandler.
  views::BubbleDialogDelegateView::CreateBubble(std::move(interception_bubble));
  return handle;
}

DiceWebSigninInterceptionBubbleView::ScopedHandle::~ScopedHandle() {
  if (!bubble_)
    return;  // The bubble was already closed, do nothing.
  views::Widget* widget = bubble_->GetWidget();
  if (!widget)
    return;
  widget->CloseWithReason(
      bubble_->GetAccepted() ? views::Widget::ClosedReason::kAcceptButtonClicked
                             : views::Widget::ClosedReason::kUnspecified);
}

DiceWebSigninInterceptionBubbleView::ScopedHandle::ScopedHandle(
    base::WeakPtr<DiceWebSigninInterceptionBubbleView> bubble)
    : bubble_(std::move(bubble)) {
  DCHECK(bubble_);
}

// static
void DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    Profile* profile,
    SigninInterceptionResult result) {
  DiceWebSigninInterceptorDelegate::RecordInterceptionResult(bubble_parameters,
                                                             profile, result);
}

bool DiceWebSigninInterceptionBubbleView::GetAccepted() const {
  return accepted_;
}

void DiceWebSigninInterceptionBubbleView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Allows the Signin Interception bubble to open links in a new tab.
  if (browser_) {
    chrome::AddWebContents(browser_.get(), source, std::move(new_contents),
                           target_url, disposition, window_features);
  }
}

DiceWebSigninInterceptionBubbleView::DiceWebSigninInterceptionBubbleView(
    Browser* browser,
    views::View* anchor_view,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      profile_keep_alive_(
          browser->profile(),
          ProfileKeepAliveOrigin::kDiceWebSigninInterceptionBubble),
      browser_(browser->AsWeakPtr()),
      profile_(browser->profile()),
      bubble_parameters_(bubble_parameters),
      callback_(std::move(callback)) {
  DCHECK(browser_);
  DCHECK(callback_);
  set_close_on_deactivate(false);

  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(browser->profile());
  web_view->LoadInitialURL(GURL(chrome::kChromeUIDiceWebSigninInterceptURL));
  web_view->GetWebContents()->SetDelegate(this);
  web_view->SetPreferredSize(
      gfx::Size(kInterceptionBubbleWidth, kInterceptionBubbleBaseHeight));
  DiceWebSigninInterceptUI* web_ui = web_view->GetWebContents()
                                         ->GetWebUI()
                                         ->GetController()
                                         ->GetAs<DiceWebSigninInterceptUI>();
  SetInitiallyFocusedView(web_view.get());
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      bubble_parameters,
      base::BindOnce(
          &DiceWebSigninInterceptionBubbleView::SetHeightAndShowWidget,
          base::Unretained(this)),
      base::BindOnce(&DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice,
                     base::Unretained(this)));
  web_view_ = web_view.get();
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void DiceWebSigninInterceptionBubbleView::SetHeightAndShowWidget(int height) {
  web_view_->SetPreferredSize(gfx::Size(kInterceptionBubbleWidth, height));
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
  GetWidget()->Show();
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptionBubbleView::GetHandle() {
  return std::make_unique<ScopedHandle>(weak_factory_.GetWeakPtr());
}

void DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice(
    SigninInterceptionUserChoice user_choice) {
  SigninInterceptionResult result;
  switch (user_choice) {
    case SigninInterceptionUserChoice::kAccept:
      result = SigninInterceptionResult::kAccepted;
      accepted_ = true;
      break;
    case SigninInterceptionUserChoice::kDecline:
      result = SigninInterceptionResult::kDeclined;
      accepted_ = false;
      break;
    case SigninInterceptionUserChoice::kGuest:
      result = SigninInterceptionResult::kAcceptedWithGuest;
      accepted_ = true;
  }

  RecordInterceptionResult(bubble_parameters_, profile_, result);
  std::move(callback_).Run(result);
  if (!accepted_) {
    // Only close the dialog when the user declined. If the user accepted the
    // dialog displays a spinner until the handle is released.
    GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCancelButtonClicked);
  }
}

content::WebContents*
DiceWebSigninInterceptionBubbleView::GetBubbleWebContentsForTesting() {
  return web_view_->GetWebContents();
}

// DiceWebSigninInterceptorDelegate --------------------------------------------

// static
bool DiceWebSigninInterceptorDelegate::IsSigninInterceptionSupportedInternal(
    const Browser& browser) {
  // Some browsers, such as web apps, don't have an avatar toolbar button to
  // anchor the bubble.
  return GetBubbleAnchorView(browser) != nullptr;
}

std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubbleInternal(
    Browser* browser,
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  DCHECK(browser);

  views::View* anchor_view = GetBubbleAnchorView(*browser);
  DCHECK(anchor_view);
  return DiceWebSigninInterceptionBubbleView::CreateBubble(
      browser, anchor_view, bubble_parameters, std::move(callback));
}

BEGIN_METADATA(DiceWebSigninInterceptionBubbleView,
               views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, Accepted)
END_METADATA
