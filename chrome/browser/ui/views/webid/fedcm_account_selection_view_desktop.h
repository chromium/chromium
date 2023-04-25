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
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/widget_observer.h"

class AccountSelectionBubbleViewInterface;

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

  // This enum is used for histograms. Do not remove or modify existing values,
  // but you may add new values at the end and increase COUNT. This enum should
  // be kept in sync with SheetType in
  // chrome/browser/ui/android/webid/AccountSelectionMediator.java as well as
  // with FedCmSheetType in tools/metrics/histograms/enums.xml.
  enum SheetType {
    ACCOUNT_SELECTION = 0,
    VERIFYING = 1,
    AUTO_REAUTHN = 2,
    SIGN_IN_TO_IDP_STATIC = 3,
    COUNT = 4
  };

  explicit FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  void Show(
      const std::string& top_frame_etld_plus_one,
      const absl::optional<std::string>& iframe_etld_plus_one,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      Account::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox) override;
  void ShowFailureDialog(
      const std::string& top_frame_etld_plus_one,
      const absl::optional<std::string>& iframe_etld_plus_one,
      const std::string& idp_etld_plus_one,
      const content::IdentityProviderMetadata& idp_metadata) override;
  std::string GetTitle() const override;
  absl::optional<std::string> GetSubtitle() const override;

  // content::WebContentsObserver
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryPageChanged(content::Page& page) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void SetInputEventActivationProtectorForTesting(
      std::unique_ptr<views::InputEventActivationProtector>);

 protected:
  friend class FedCmAccountSelectionViewBrowserTest;

  // Creates the bubble. Sets the bubble's accessible title. Registers any
  // observers.
  virtual views::Widget* CreateBubbleWithAccessibleTitle(
      const std::u16string& top_frame_etld_plus_one,
      const absl::optional<std::u16string>& iframe_etld_plus_one,
      const absl::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      bool show_auto_reauthn_checkbox);

  // Returns AccountSelectionBubbleViewInterface for bubble views::Widget.
  virtual AccountSelectionBubbleViewInterface* GetBubbleView();
  virtual const AccountSelectionBubbleViewInterface* GetBubbleView() const;

 private:
  enum class State {
    // User is shown message that they are not currently signed-in to IdP.
    // Dialog has button to sign-in to IdP.
    IDP_SIGNIN_STATUS_MISMATCH,

    // User is shown list of accounts they have with IDP and is prompted to
    // select an account.
    ACCOUNT_PICKER,

    // User is prompted to grant permission for specific account they have with
    // IDP to communicate with RP.
    PERMISSION,

    // Shown after the user has granted permission while the id token is being
    // fetched.
    VERIFYING,

    // Shown when the user is being shown a dialog that auto re-authn is
    // happening.
    AUTO_REAUTHN
  };

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AccountSelectionBubbleView::Observer:
  void OnAccountSelected(const Account& account,
                         const IdentityProviderDisplayData& idp_display_data,
                         const ui::Event& event) override;
  void OnLinkClicked(LinkType link_type,
                     const GURL& url,
                     const ui::Event& event) override;
  void OnBackButtonClicked() override;
  void OnCloseButtonClicked(const ui::Event& event) override;
  void ShowModalDialogView(const GURL& url) override;

  void ShowVerifyingSheet(const Account& account,
                          const IdentityProviderDisplayData& idp_display_data);

  // Returns the SheetType to be used for metrics reporting.
  SheetType GetSheetType();

  // Closes the widget and notifies the delegate.
  void Close();

  // Notify the delegate that the widget was closed with reason
  // `dismiss_reason`.
  void OnDismiss(
      content::IdentityRequestDialogController::DismissReason dismiss_reason);

  std::vector<IdentityProviderDisplayData> idp_display_data_list_;

  std::u16string top_frame_for_display_;

  absl::optional<std::u16string> iframe_for_display_;

  State state_{State::ACCOUNT_PICKER};

  // Whether to notify the delegate when the widget is closed.
  bool notify_delegate_of_dismiss_{true};

  base::WeakPtr<views::Widget> bubble_widget_;

  std::unique_ptr<views::InputEventActivationProtector> input_protector_;

  base::WeakPtrFactory<FedCmAccountSelectionView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
