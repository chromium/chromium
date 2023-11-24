// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include "chrome/browser/ui/webid/account_selection_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/widget_observer.h"

using TokenError = content::IdentityCredentialTokenError;

class AccountSelectionBubbleViewInterface;

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
class FedCmAccountSelectionView : public AccountSelectionView,
                                  public AccountSelectionBubbleView::Observer,
                                  public FedCmModalDialogView::Observer,
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
    SIGN_IN_ERROR = 4,
    COUNT = 5
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
      const blink::mojom::RpContext& rp_context,
      const content::IdentityProviderMetadata& idp_metadata) override;
  void ShowErrorDialog(const std::string& top_frame_etld_plus_one,
                       const absl::optional<std::string>& iframe_etld_plus_one,
                       const std::string& idp_etld_plus_one,
                       const blink::mojom::RpContext& rp_context,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const absl::optional<TokenError>& error) override;
  std::string GetTitle() const override;
  absl::optional<std::string> GetSubtitle() const override;

  // FedCmModalDialogView::Observer
  void OnPopupWindowDestroyed() override;

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
  void SetIdpSigninPopupWindowForTesting(std::unique_ptr<FedCmModalDialogView>);

  // AccountSelectionBubbleView::Observer:
  content::WebContents* ShowModalDialog(const GURL& url) override;
  void CloseModalDialog() override;

 protected:
  friend class FedCmAccountSelectionViewBrowserTest;

  // Creates the bubble. Sets the bubble's accessible title. Registers any
  // observers. May fail and return nullptr if there is no browser or tab strip
  // model.
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
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           MismatchDialogDismissedByCloseIconMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           MismatchDialogDismissedForOtherReasonsMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           MismatchDialogContinueClickedMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           MismatchDialogDestroyedMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           MismatchDialogContinueClickedThenDestroyedMetric);
  FRIEND_TEST_ALL_PREFIXES(
      FedCmAccountSelectionViewDesktopTest,
      IdpSigninStatusAccountsReceivedAndNoPopupClosedByIdpMetric);
  FRIEND_TEST_ALL_PREFIXES(
      FedCmAccountSelectionViewDesktopTest,
      IdpSigninStatusAccountsNotReceivedAndPopupClosedByIdpMetric);
  FRIEND_TEST_ALL_PREFIXES(
      FedCmAccountSelectionViewDesktopTest,
      IdpSigninStatusAccountsNotReceivedAndNoPopupClosedByIdpMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           IdpSigninStatusPopupClosedBeforeAccountsPopulated);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           IdpSigninStatusPopupClosedAfterAccountsPopulated);

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
    AUTO_REAUTHN,

    // Shown when an error has occurred during the user's sign-in attempt and
    // IDP has not provided any details on the failure.
    SIGN_IN_ERROR
  };

  // This enum describes the outcome of the mismatch dialog and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end. This enum should be kept in sync with
  // FedCmMismatchDialogResult in tools/metrics/histograms/enums.xml.
  enum class MismatchDialogResult {
    kContinued,
    kDismissedByCloseIcon,
    kDismissedForOtherReasons,

    kMaxValue = kDismissedForOtherReasons
  };

  // This enum describes the outcome of the pop-up window and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end. This enum should be kept in sync with
  // FedCmPopupWindowResult in tools/metrics/histograms/enums.xml.
  enum class PopupWindowResult {
    kAccountsReceivedAndPopupClosedByIdp,
    kAccountsReceivedAndPopupNotClosedByIdp,
    kAccountsNotReceivedAndPopupClosedByIdp,
    kAccountsNotReceivedAndPopupNotClosedByIdp,

    kMaxValue = kAccountsNotReceivedAndPopupNotClosedByIdp
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
  void OnLoginToIdP(const GURL& idp_login_url, const ui::Event& event) override;
  void OnGotIt(const ui::Event& event) override;
  void OnMoreDetails(const ui::Event& event) override;

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

  std::unique_ptr<FedCmModalDialogView> popup_window_;

  // If dialog has been populated with accounts as a result of the IDP sign-in
  // flow but the IDP sign-in pop-up window has not been closed yet, we use this
  // callback to show the accounts dialog upon closing the IDP sign-in pop-up
  // window. This can happen when IDP sign-in status header is sent after the
  // sign-in flow is complete but the pop-up window is not closed yet e.g. user
  // is asked to verify phone number, change password, etc.
  base::OnceClosure show_accounts_dialog_callback_;

  // If dialog has NOT been populated with accounts yet as a result of the IDP
  // sign-in flow and the IDP sign-in pop-up window has been closed, we use this
  // boolean to let bubble widget know it should unhide itself when the dialog
  // is ready. This can happen when the accounts fetch has yet to finish but the
  // pop-up window has already been closed.
  bool is_modal_closed_but_accounts_fetch_pending_{false};

  // If IDP sign-in pop-up window is closed through means other than
  // IdentityProvider.close() such as the user closing the pop-up window or
  // window.close(), we should destroy the bubble widget and reject the
  // navigator.credentials.get promise. This boolean tracks whether
  // IdentityProvider.close() was called.
  bool should_destroy_bubble_widget_{true};

  // Whether the associated WebContents is visible or not.
  bool is_web_contents_visible_;

  // Whether the "Continue" button on the mismatch dialog is clicked. Once the
  // "Continue" button is clicked, a pop-up window is shown for the user to sign
  // in to an IDP. The mismatch dialog is hidden until it has been updated into
  // an accounts dialog, which occurs after the user completes the sign in flow.
  // If the user closes the page with the hidden mismatch dialog before
  // completing the flow, this boolean prevents us from double counting the
  // Blink.FedCm.IdpSigninStatus.MismatchDialogResult metric.
  bool is_mismatch_continue_clicked_{false};

  // Time when IdentityProvider.close() was called for metrics purposes.
  base::TimeTicks idp_close_popup_time_;

  // The current state of the IDP sign-in pop-up window, if initiated by user.
  PopupWindowResult popup_window_state_;

  base::WeakPtrFactory<FedCmAccountSelectionView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
