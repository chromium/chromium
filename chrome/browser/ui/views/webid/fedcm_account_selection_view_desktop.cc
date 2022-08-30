// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
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
    const std::string& idp_etld_plus_one,
    const std::vector<Account>& accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    Account::SignInMode sign_in_mode) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents());
  // `browser` is null in unit tests.
  if (browser)
    browser->tab_strip_model()->AddObserver(this);

  idp_etld_plus_one_ = base::UTF8ToUTF16(idp_etld_plus_one);
  idp_metadata_ = idp_metadata;
  client_data_ = std::make_unique<content::ClientIdData>(client_data);
  account_list_ = std::vector<content::IdentityRequestAccount>(accounts.begin(),
                                                               accounts.end());
  state_ =
      (account_list_.size() == 1u) ? State::PERMISSION : State::ACCOUNT_PICKER;

  bubble_widget_ = CreateBubble(browser, base::UTF8ToUTF16(rp_etld_plus_one),
                                idp_etld_plus_one_)
                       ->GetWeakPtr();
  GetBubbleView()->ShowAccountPicker(idp_etld_plus_one_,
                                     /*show_back_button=*/false, accounts,
                                     idp_metadata, client_data);
  bubble_widget_->Show();
  bubble_widget_->AddObserver(this);
}

void FedCmAccountSelectionView::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!bubble_widget_)
    return;

  if (visibility == content::Visibility::VISIBLE) {
    bubble_widget_->Show();
  } else {
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
    const std::u16string& idp_etld_plus_one) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view = browser_view->contents_web_view();

  return views::BubbleDialogDelegateView::CreateBubble(
      new AccountSelectionBubbleView(rp_etld_plus_one, idp_etld_plus_one,
                                     anchor_view,
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
    const std::string& account_id) {
  auto account_list_it = std::find_if(
      account_list_.begin(), account_list_.end(),
      [&account_id](const content::IdentityRequestAccount& account) {
        return account.id == account_id;
      });
  DCHECK(account_list_it != account_list_.end());

  state_ = (state_ == State::ACCOUNT_PICKER &&
            account_list_it->login_state == Account::LoginState::kSignUp)
               ? State::PERMISSION
               : State::VERIFYING;
  if (state_ == State::VERIFYING) {
    notify_delegate_of_dismiss_ = false;
    delegate_->OnAccountSelected(*account_list_it);

    GetBubbleView()->ShowVerifyingSheet(*account_list_it, idp_metadata_);
    return;
  }

  GetBubbleView()->ShowAccountPicker(
      idp_etld_plus_one_,
      /*show_back_button=*/true,
      std::vector<content::IdentityRequestAccount>({*account_list_it}),
      idp_metadata_, *client_data_);
}

void FedCmAccountSelectionView::OnLinkClicked(const GURL& url) {
  Browser* browser =
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents());
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  DCHECK(tab_strip_model);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model->delegate()->AddTabAt(url, -1, true);
}

void FedCmAccountSelectionView::OnBackButtonClicked() {
  state_ = State::ACCOUNT_PICKER;
  GetBubbleView()->ShowAccountPicker(idp_etld_plus_one_,
                                     /*show_back_button=*/false, account_list_,
                                     idp_metadata_, *client_data_);
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
