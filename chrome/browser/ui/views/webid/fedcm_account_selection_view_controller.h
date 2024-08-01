// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

// Class that owns the FedCmAccountSelectionView, but is itself owned by
// TabFeatures.
class FedCmAccountSelectionViewController {
 public:
  explicit FedCmAccountSelectionViewController(tabs::TabInterface* tab);
  ~FedCmAccountSelectionViewController();

  std::unique_ptr<FedCmAccountSelectionView> CreateAccountSelectionView(
      AccountSelectionView::Delegate* delegate);

 private:
  base::WeakPtr<FedCmAccountSelectionView> account_selection_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
