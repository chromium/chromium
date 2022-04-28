// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<FedCmAccountSelectionView>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize() {
  // TODO(crbug.com/1311482): check that this hardcoded value makes sense even
  // on high-dpi desktops.
  return 20;
}

// static
int AccountSelectionView::GetBrandIconIdealSize() {
  // TODO(crbug.com/1311482): check that this hardcoded value makes sense even
  // on high-dpi desktops.
  return 20;
}

FedCmAccountSelectionView::FedCmAccountSelectionView(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate),
      content::WebContentsObserver(delegate->GetWebContents()) {}

FedCmAccountSelectionView::~FedCmAccountSelectionView() {
  if (bubble_widget_) {
    bubble_widget_->Close();
  }
}

void FedCmAccountSelectionView::Show(
    const std::string& rp_etld_plus_one,
    const std::string& idp_etld_plus_one,
    base::span<const Account> accounts,
    const content::IdentityProviderMetadata& idp_metadata,
    const content::ClientIdData& client_data,
    Account::SignInMode sign_in_mode) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
      chrome::FindBrowserWithWebContents(delegate_->GetWebContents()));
  views::View* anchor_view = browser_view->top_container();
  TabStripModel* tab_strip_model = browser_view->browser()->tab_strip_model();
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(
                       new AccountSelectionBubbleView(
                           delegate_, rp_etld_plus_one, idp_etld_plus_one,
                           accounts, idp_metadata, client_data, anchor_view,
                           SystemNetworkContextManager::GetInstance()
                               ->GetSharedURLLoaderFactory(),
                           tab_strip_model))
                       ->GetWeakPtr();
  bubble_widget_->Show();
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

void FedCmAccountSelectionView::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  // Close the bubble when the user navigates within the same tab.
  if (bubble_widget_) {
    bubble_widget_->Close();
    bubble_widget_ = nullptr;
  }
}
