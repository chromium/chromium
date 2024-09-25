// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_controller.h"

FedCmAccountSelectionViewController::FedCmAccountSelectionViewController(
    tabs::TabInterface* tab)
    : tab_(tab) {
  tab_subscriptions_.push_back(tab_->RegisterDidEnterForeground(
      base::BindRepeating(&FedCmAccountSelectionViewController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_->RegisterWillEnterBackground(base::BindRepeating(
          &FedCmAccountSelectionViewController::TabWillEnterBackground,
          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(
      tab_->RegisterWillDiscardContents(base::BindRepeating(
          &FedCmAccountSelectionViewController::WillDiscardContents,
          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(
      base::BindRepeating(&FedCmAccountSelectionViewController::WillDetach,
                          weak_factory_.GetWeakPtr())));
}

FedCmAccountSelectionViewController::~FedCmAccountSelectionViewController() {}

void FedCmAccountSelectionViewController::TabForegrounded(
    tabs::TabInterface* tab) {
  if (account_selection_view_) {
    account_selection_view_->OnTabForegrounded();
  }
}

void FedCmAccountSelectionViewController::TabWillEnterBackground(
    tabs::TabInterface* tab) {
  if (account_selection_view_) {
    account_selection_view_->OnTabBackgrounded();
  }
}

void FedCmAccountSelectionViewController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  if (account_selection_view_) {
    account_selection_view_->Close();
  }
}

void FedCmAccountSelectionViewController::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // Close the UI and reject the call if detached, otherwise the dialog will be
  // shown in the wrong tab.
  if (account_selection_view_) {
    account_selection_view_->Close();
  }
}

std::unique_ptr<FedCmAccountSelectionView>
FedCmAccountSelectionViewController::CreateAccountSelectionView(
    AccountSelectionView::Delegate* delegate) {
  auto account_selection_view =
      std::make_unique<FedCmAccountSelectionView>(delegate);
  if (tab_->IsInForeground()) {
    account_selection_view->OnTabForegrounded();
  } else {
    account_selection_view->OnTabBackgrounded();
  }
  account_selection_view_ = account_selection_view->GetWeakPtr();
  return account_selection_view;
}
