// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include "chrome/browser/ui/webid/account_selection_view.h"

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
class FedCmAccountSelectionView : public AccountSelectionView,
                                  content::WebContentsObserver,
                                  TabStripModelObserver,
                                  views::WidgetObserver {
 public:
  explicit FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  void Show(const std::string& rp_etld_plus_one,
            const std::string& idp_etld_plus_one,
            base::span<const Account> accounts,
            const content::IdentityProviderMetadata& idp_metadata,
            const content::ClientIdData& client_data,
            Account::SignInMode sign_in_mode) override;

  // content::WebContentsObserver
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 protected:
  friend class FedCmAccountSelectionViewBrowserTest;

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Called when the user selected an account AND granted consent.
  void OnAccountSelected(const content::IdentityRequestAccount& account);

  // Closes the widget and notifies the delegate.
  void Close();

  // Notify the delegate that the widget was closed.
  // |should_embargo| indicates whether the FedCM API should be embargoed due
  // to the user explicitly dismissing the dialog.
  void OnDismiss(bool should_embargo);

  // Whether to notify the delegate when the widget is closed.
  bool notify_delegate_of_dismiss_{true};

  base::WeakPtr<views::Widget> bubble_widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
