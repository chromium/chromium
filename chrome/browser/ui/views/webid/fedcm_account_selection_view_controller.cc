// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_controller.h"

FedCmAccountSelectionViewController::FedCmAccountSelectionViewController(
    tabs::TabInterface* tab) {
  // TODO(crbug.com/348177116: add tab subscriptions)
}

FedCmAccountSelectionViewController::~FedCmAccountSelectionViewController() {}

std::unique_ptr<FedCmAccountSelectionView>
FedCmAccountSelectionViewController::CreateAccountSelectionView(
    AccountSelectionView::Delegate* delegate) {
  auto account_selection_view =
      std::make_unique<FedCmAccountSelectionView>(delegate);
  account_selection_view_ = account_selection_view->GetWeakPtr();
  return account_selection_view;
}
