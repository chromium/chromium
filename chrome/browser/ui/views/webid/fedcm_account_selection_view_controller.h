// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_

#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
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
  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Called when the tab will be removed from the window.
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  base::WeakPtr<FedCmAccountSelectionView> account_selection_view_;

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Must be the last member.
  base::WeakPtrFactory<FedCmAccountSelectionViewController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_CONTROLLER_H_
