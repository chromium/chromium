// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/dice_web_signin_interception_bubble_view.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr int kInterceptionBubbleHeight = 342;
constexpr int kInterceptionBubbleWidth = 290;

// Returns true if the account is managed.
bool IsManaged(const AccountInfo& account_info) {
  return !account_info.hosted_domain.empty() &&
         account_info.hosted_domain != kNoHostedDomainFound;
}

}  // namespace

DiceWebSigninInterceptionBubbleView::~DiceWebSigninInterceptionBubbleView() {
  // Cancel if the bubble is destroyed without user interaction.
  if (callback_) {
    RecordInterceptionResult(bubble_parameters_, profile_,
                             SigninInterceptionResult::kIgnored);
    std::move(callback_).Run(false);
  }
}

// static
void DiceWebSigninInterceptionBubbleView::CreateBubble(
    Profile* profile,
    views::View* anchor_view,
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(bool)> callback) {
  // The widget is owned by the views system.
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      new DiceWebSigninInterceptionBubbleView(
          profile, anchor_view, bubble_parameters, std::move(callback)));
  // TODO(droger): Delay showing the bubble until the web view is loaded.
  widget->Show();
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
      histogram_base_name.append(".Enterprise");
      break;
    case DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser:
      histogram_base_name.append(".MultiUser");
      break;
    case DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch:
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
    if (IsManaged(bubble_parameters.intercepted_account)) {
      std::string histogram_name = histogram_base_name + ".NewIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
    if (IsManaged(bubble_parameters.primary_account)) {
      std::string histogram_name = histogram_base_name + ".PrimaryIsEnterprise";
      base::UmaHistogramEnumeration(histogram_name, result);
    }
  }
}

DiceWebSigninInterceptionBubbleView::DiceWebSigninInterceptionBubbleView(
    Profile* profile,
    views::View* anchor_view,
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(bool)> callback)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      profile_(profile),
      bubble_parameters_(bubble_parameters),
      callback_(std::move(callback)) {
  DCHECK(profile_);
  set_close_on_deactivate(false);

  // Create the web view in the native bubble.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile);
  web_view->LoadInitialURL(GURL(chrome::kChromeUIDiceWebSigninInterceptURL));
  web_view->SetPreferredSize(
      gfx::Size(kInterceptionBubbleWidth, kInterceptionBubbleHeight));
  DiceWebSigninInterceptUI* web_ui = web_view->GetWebContents()
                                         ->GetWebUI()
                                         ->GetController()
                                         ->GetAs<DiceWebSigninInterceptUI>();
  DCHECK(web_ui);
  // Unretained is fine because this outlives the inner web UI.
  web_ui->Initialize(
      bubble_parameters,
      base::BindOnce(&DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice,
                     base::Unretained(this)));
  AddChildView(std::move(web_view));

  set_margins(gfx::Insets());
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void DiceWebSigninInterceptionBubbleView::OnWebUIUserChoice(bool accept) {
  RecordInterceptionResult(bubble_parameters_, profile_,
                           accept ? SigninInterceptionResult::kAccepted
                                  : SigninInterceptionResult::kDeclined);
  std::move(callback_).Run(accept);
  GetWidget()->CloseWithReason(
      accept ? views::Widget::ClosedReason::kAcceptButtonClicked
             : views::Widget::ClosedReason::kCancelButtonClicked);
}

// DiceWebSigninInterceptorDelegate --------------------------------------------

void DiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubbleInternal(
    Browser* browser,
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(browser);

  if (bubble_parameters.interception_type ==
          DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch &&
      !base::FeatureList::IsEnabled(features::kProfilesUIRevamp)) {
    // The bubble for profile switch is not enabled.
    DiceWebSigninInterceptionBubbleView::RecordInterceptionResult(
        bubble_parameters, browser->profile(),
        DiceWebSigninInterceptionBubbleView::SigninInterceptionResult::
            kNotDisplayed);
    std::move(callback).Run(false);
    return;
  }

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAvatarToolbarButton();
  DCHECK(anchor_view);
  DiceWebSigninInterceptionBubbleView::CreateBubble(
      browser->profile(), anchor_view, bubble_parameters, std::move(callback));
}
