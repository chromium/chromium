// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

using DismissReason = content::IdentityRequestDialogController::DismissReason;

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<FedCmAccountSelectionView>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize() {
  return 20 / FedCmAccountSelectionView::kMaskableWebIconSafeZoneRatio;
}

// static
int AccountSelectionView::GetBrandIconIdealSize() {
  // As only a single brand icon is selected and the user can have monitors with
  // different screen densities, make the ideal size be the size which works
  // with a high density display (if the OS supports high density displays).
  float max_supported_scale = ui::GetScaleForResourceScaleFactor(
      ui::GetSupportedResourceScaleFactors().back());
  return round(GetBrandIconMinimumSize() * max_supported_scale);
}

FedCmAccountSelectionView::FedCmAccountSelectionView(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate),
      content::WebContentsObserver(delegate->GetWebContents()) {}

FedCmAccountSelectionView::~FedCmAccountSelectionView() {
  notify_delegate_of_dismiss_ = false;
  Close();

  TabStripModelObserver::StopObservingAll(this);
}

void FedCmAccountSelectionView::Show(
    const std::string& rp_etld_plus_one,
    const std::vector<content::IdentityProviderData>& identity_provider_data,
    Account::SignInMode sign_in_mode) {
  // Either Show or ShowFailureDialog has already been called for other IDPs
  // from the same token request. This could happen when accounts fetch fails
  // for some IDPs. We have yet to support the multi IDP case where not all IDPs
  // are successful. The early return causes follow up Show calls to be ignored.
  if (bubble_widget_)
    return;

  Browser* browser =
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents());
  // `browser` is null in unit tests.
  if (browser)
    browser->tab_strip_model()->AddObserver(this);

  size_t accounts_size = 0u;
  for (const auto& identity_provider : identity_provider_data) {
    idp_data_list_.emplace_back(
        base::UTF8ToUTF16(identity_provider.idp_for_display),
        identity_provider.idp_metadata, identity_provider.client_metadata,
        identity_provider.accounts);
    accounts_size += identity_provider.accounts.size();
  }
  state_ = accounts_size == 1u ? State::PERMISSION : State::ACCOUNT_PICKER;

  absl::optional<std::u16string> idp_title =
      identity_provider_data.size() == 1u
          ? absl::make_optional<std::u16string>(
                base::UTF8ToUTF16(identity_provider_data[0].idp_for_display))
          : absl::nullopt;
  rp_for_display_ = base::UTF8ToUTF16(rp_etld_plus_one);
  bubble_widget_ =
      CreateBubble(browser, rp_for_display_, idp_title)->GetWeakPtr();
  GetBubbleView()->ShowAccountPicker(idp_data_list_,
                                     /*show_back_button=*/false);
  bubble_widget_->Show();
  bubble_widget_->AddObserver(this);
}

void FedCmAccountSelectionView::ShowFailureDialog(
    const std::string& rp_etld_plus_one,
    const std::string& idp_etld_plus_one) {
  // Either Show or ShowFailureDialog has already been called for other IDPs
  // from the same token request. This could happen when accounts fetch fails
  // for some IDPs. We have yet to support the multi IDP case where not all IDPs
  // are successful. The early return causes follow up ShowFailureDialog calls
  // to be ignored.
  if (bubble_widget_)
    return;

  Browser* browser =
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents());
  // `browser` is null in unit tests.
  if (browser)
    browser->tab_strip_model()->AddObserver(this);

  bubble_widget_ = CreateBubble(browser, base::UTF8ToUTF16(rp_etld_plus_one),
                                base::UTF8ToUTF16(idp_etld_plus_one))
                       ->GetWeakPtr();
  GetBubbleView()->ShowFailureDialog(base::UTF8ToUTF16(rp_etld_plus_one),
                                     base::UTF8ToUTF16(idp_etld_plus_one));
  bubble_widget_->Show();
  bubble_widget_->AddObserver(this);
}

void FedCmAccountSelectionView::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!bubble_widget_)
    return;

  if (visibility == content::Visibility::VISIBLE) {
    bubble_widget_->widget_delegate()->SetCanActivate(true);
    bubble_widget_->Show();
  } else {
    // On Mac, NativeWidgetMac::Activate() ignores the views::Widget visibility.
    // Make the views::Widget non-activatable while it is hidden to prevent the
    // views::Widget from being shown during focus traversal.
    // TODO(crbug.com/1367309): fix the issue on Mac.
    bubble_widget_->widget_delegate()->SetCanActivate(false);
    bubble_widget_->Hide();
  }
}

void FedCmAccountSelectionView::PrimaryPageChanged(content::Page& page) {
  // Close the bubble when the user navigates within the same tab.
  Close();
}

void FedCmAccountSelectionView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  int index =
      tab_strip_model->GetIndexOfWebContents(delegate_->GetWebContents());
  // If the WebContents has been moved out of this `tab_strip_model`, close the
  // bubble.
  // TODO(npm): we should change the management logic so that it is
  // possible to move the bubble with the tab, even to a different browser
  // window.
  if (index == TabStripModel::kNoTab && bubble_widget_) {
    Close();
  }
}

views::Widget* FedCmAccountSelectionView::CreateBubble(
    Browser* browser,
    const std::u16string& rp_etld_plus_one,
    const absl::optional<std::u16string>& idp_title) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view = browser_view->contents_web_view();

  return views::BubbleDialogDelegateView::CreateBubble(
      new AccountSelectionBubbleView(rp_etld_plus_one, idp_title, anchor_view,
                                     SystemNetworkContextManager::GetInstance()
                                         ->GetSharedURLLoaderFactory(),
                                     this));
}

AccountSelectionBubbleViewInterface*
FedCmAccountSelectionView::GetBubbleView() {
  return static_cast<AccountSelectionBubbleView*>(
      bubble_widget_->widget_delegate());
}

void FedCmAccountSelectionView::OnWidgetDestroying(views::Widget* widget) {
  DismissReason dismiss_reason =
      (bubble_widget_->closed_reason() ==
       views::Widget::ClosedReason::kCloseButtonClicked)
          ? DismissReason::CLOSE_BUTTON
          : DismissReason::OTHER;
  OnDismiss(dismiss_reason);
}

void FedCmAccountSelectionView::OnAccountSelected(
    const Account& account,
    const IdentityProviderDisplayData& idp_data) {
  state_ = (state_ == State::ACCOUNT_PICKER &&
            account.login_state == Account::LoginState::kSignUp)
               ? State::PERMISSION
               : State::VERIFYING;
  if (state_ == State::VERIFYING) {
    notify_delegate_of_dismiss_ = false;

    base::WeakPtr<FedCmAccountSelectionView> weak_ptr(
        weak_ptr_factory_.GetWeakPtr());
    delegate_->OnAccountSelected(idp_data.idp_metadata_.config_url, account);
    // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
    // See https://crbug.com/1393650 for details.
    if (!weak_ptr)
      return;

    GetBubbleView()->ShowVerifyingSheet(account, idp_data);
    return;
  }
  GetBubbleView()->ShowSingleAccountConfirmDialog(rp_for_display_, account,
                                                  idp_data);
}

void FedCmAccountSelectionView::OnLinkClicked(LinkType link_type,
                                              const GURL& url) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents());
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  DCHECK(tab_strip_model);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model->delegate()->AddTabAt(url, -1, true);

  switch (link_type) {
    case LinkType::TERMS_OF_SERVICE:
      UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.SignUp.TermsOfServiceClicked", true);
      break;

    case LinkType::PRIVACY_POLICY:
      UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.SignUp.PrivacyPolicyClicked", true);
      break;
  }
}

void FedCmAccountSelectionView::OnBackButtonClicked() {
  state_ = State::ACCOUNT_PICKER;
  GetBubbleView()->ShowAccountPicker(idp_data_list_,
                                     /*show_back_button=*/false);
}

void FedCmAccountSelectionView::OnCloseButtonClicked() {
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.CloseVerifySheet.Desktop",
                        state_ == State::VERIFYING);
  bubble_widget_->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void FedCmAccountSelectionView::Close() {
  if (!bubble_widget_)
    return;

  bubble_widget_->Close();
  OnDismiss(DismissReason::OTHER);
}

void FedCmAccountSelectionView::OnDismiss(DismissReason dismiss_reason) {
  if (!bubble_widget_)
    return;

  bubble_widget_->RemoveObserver(this);
  bubble_widget_.reset();

  if (notify_delegate_of_dismiss_)
    delegate_->OnDismiss(dismiss_reason);
}
