// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
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
constexpr int kInterceptionBubbleWithoutGuestHeight = 326;
constexpr int kInterceptionBubbleGuestFooterHeight = 36;
constexpr int kInterceptionBubbleManagedDisclaimerHeight = 52;
constexpr int kInterceptionBubbleExtraTextHeight = 30;
constexpr int kInterceptionBubbleWidth = 290;

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
std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptionBubbleView::CreateBubble(
    Browser* browser,
    views::View* anchor_view,
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  auto interception_bubble =
      base::WrapUnique(new DiceWebSigninInterceptionBubbleView(
          browser, anchor_view, bubble_parameters, std::move(callback)));
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle> handle =
      interception_bubble->GetHandle();
  // The widget is owned by the views system.
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(interception_bubble));
  // TODO(droger): Delay showing the bubble until the web view is loaded.
  widget->Show();
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
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    Profile* profile,
    SigninInterceptionResult result) {
  std::string histogram_base_name = "Signin.InterceptResult";
  switch (bubble_parameters.interception_type) {
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise:
    case DiceWebSigninInterceptor::SigninInterceptionType::
        kEnterpriseAcceptManagement:
    case DiceWebSigninInterceptor::SigninInterceptionType::kEnterpriseForced:
      histogram_base_name.append(".Enterprise");
      break;
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
      histogram_base_name.append(".MultiUser");
      break;
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced:
      histogram_base_name.append(".Switch");
      break;
  }

  // Record aggregated histogram for each interception type.
  base::UmaHistogramEnumeration(histogram_base_name, result);
  // Record histogram sliced by Sync status.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  std::string sync_suffix =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)
          ? ".Sync"
          : ".NoSync";
  base::UmaHistogramEnumeration(histogram_base_name + sync_suffix, result);
  // For Enterprise, slice per enterprise status for each account.
  if (bubble_parameters.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kEnterprise) {
    if (bubble_parameters.intercepted_account.IsManaged()) {
      std::string histogram_name = histogram_base_name + ".NewIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
    if (bubble_parameters.primary_account.IsManaged()) {
      std::string histogram_name = histogram_base_name + ".PrimaryIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
  }
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
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
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
  int height = kInterceptionBubbleWithoutGuestHeight;
  if (bubble_parameters.show_guest_option)
    height += kInterceptionBubbleGuestFooterHeight;
  if (bubble_parameters.interception_type ==
      DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser) {
    // The kMultiUser bubble has a longer text, increase the height a bit.
    // TODO: Dynamically compute the right size based on the text length.
    height += kInterceptionBubbleExtraTextHeight;
  }
  if (bubble_parameters.show_managed_disclaimer) {
    // Increase the height to display an entreprise disclaimer for managed
    // profile.
    height += kInterceptionBubbleManagedDisclaimerHeight;
  }
  web_view->SetPreferredSize(gfx::Size(kInterceptionBubbleWidth, height));
  DiceWebSigninInterceptUI* web_ui = web_view->GetWebContents()
                                         ->GetWebUI()
                                         ->GetController()
                                         ->GetAs<DiceWebSigninInterceptUI>();
  SetInitiallyFocusedView(web_view.get());
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      bubble_parameters,
      base::BindOnce(&DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice,
                     base::Unretained(this)));
  web_view_ = web_view.get();
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
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

std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubbleInternal(
    Browser* browser,
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(SigninInterceptionResult)> callback) {
  DCHECK(browser);

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);
  return DiceWebSigninInterceptionBubbleView::CreateBubble(
      browser, anchor_view, bubble_parameters, std::move(callback));
}

BEGIN_METADATA(DiceWebSigninInterceptionBubbleView,
               views::BubbleDialogDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, Accepted)
END_METADATA
