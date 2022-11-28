// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include "chrome/browser/ui/webid/account_selection_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget_observer.h"

class AccountSelectionBubbleViewInterface;
class Browser;

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
class FedCmAccountSelectionView : public AccountSelectionView,
                                  public AccountSelectionBubbleView::Observer,
                                  content::WebContentsObserver,
                                  TabStripModelObserver,
                                  views::WidgetObserver {
 public:
  // safe_zone_diameter/icon_size as defined in
  // https://www.w3.org/TR/appmanifest/#icon-masks
  static constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

  explicit FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  void Show(
      const std::string& rp_etld_plus_one,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      Account::SignInMode sign_in_mode) override;
  void ShowFailureDialog(const std::string& rp_etld_plus_one,
                         const std::string& idp_etld_plus_one) override;

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

  // Creates bubble views::Widget.
  virtual views::Widget* CreateBubble(
      Browser* browser,
      const std::u16string& rp_etld_plus_one,
      const absl::optional<std::u16string>& idp_title);

  // Returns AccountSelectionBubbleViewInterface for bubble views::Widget.
  virtual AccountSelectionBubbleViewInterface* GetBubbleView();

 private:
  enum class State {
    // User is shown list of accounts they have with IDP and is prompted to
    // select an account.
    ACCOUNT_PICKER,

    // User is prompted to grant permission for specific account they have with
    // IDP to communicate with RP.
    PERMISSION,

    // Shown after the user has granted permission while the id token is being
    // fetched.
    VERIFYING
  };

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AccountSelectionBubbleView::Observer:
  void OnAccountSelected(const Account& account,
                         const IdentityProviderDisplayData& idp_data) override;
  void OnLinkClicked(LinkType link_type, const GURL& url) override;
  void OnBackButtonClicked() override;
  void OnCloseButtonClicked() override;

  // Called when the user selected an account AND granted consent.
  void OnAccountSelected(const content::IdentityRequestAccount& account);

  // Closes the widget and notifies the delegate.
  void Close();

  // Notify the delegate that the widget was closed with reason
  // `dismiss_reason`.
  void OnDismiss(
      content::IdentityRequestDialogController::DismissReason dismiss_reason);

  std::vector<IdentityProviderDisplayData> idp_data_list_;

  std::u16string rp_for_display_;

  State state_{State::ACCOUNT_PICKER};

  // Whether to notify the delegate when the widget is closed.
  bool notify_delegate_of_dismiss_{true};

  base::WeakPtr<views::Widget> bubble_widget_;

  base::WeakPtrFactory<FedCmAccountSelectionView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
