// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/widget_observer.h"

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

class AccountSelectionViewBase;

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
class FedCmAccountSelectionView : public AccountSelectionView,
                                  public AccountSelectionViewBase::Observer,
                                  public FedCmModalDialogView::Observer,
                                  content::WebContentsObserver,
                                  views::WidgetObserver {
 public:
  // safe_zone_diameter/icon_size as defined in
  // https://www.w3.org/TR/appmanifest/#icon-masks
  static constexpr float kMaskableWebIconSafeZoneRatio = 0.8f;

  enum class DialogType {
    // FedCM dialog inherits a bubble dialog, which is typically shown on the
    // top-right corner of the browser. The user can switch tabs and interact
    // with web contents.
    BUBBLE,

    // FedCM dialog inherits a modal dialog, which is typically shown in the
    // middle of the browser overlapping the line of death. The user can switch
    // tabs but cannot interact with web contents.
    MODAL
  };

  explicit FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  bool Show(
      const std::string& rp_for_display,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      Account::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts) override;
  bool ShowFailureDialog(
      const std::string& rp_for_display,
      const std::string& idp_etld_plus_one,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata) override;
  bool ShowErrorDialog(const std::string& rp_for_display,
                       const std::string& idp_etld_plus_one,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override;
  bool ShowLoadingDialog(const std::string& rp_for_display,
                         const std::string& idp_etld_plus_one,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode) override;
  void OnAccountsDisplayed() override;

  void ShowUrl(LinkType link_type, const GURL& url) override;
  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;
  content::WebContents* GetRpWebContents() override;

  // FedCmModalDialogView::Observer
  void OnPopupWindowDestroyed() override;

  void OnTabForegrounded();
  void OnTabBackgrounded();
  // Closes the widget and notifies the delegate.
  void Close();

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  void SetInputEventActivationProtectorForTesting(
      std::unique_ptr<views::InputEventActivationProtector>);
  void SetIdpSigninPopupWindowForTesting(std::unique_ptr<FedCmModalDialogView>);

  // AccountSelectionBubbleView::Observer:
  content::WebContents* ShowModalDialog(const GURL& url,
                                        blink::mojom::RpMode rp_mode) override;
  void CloseModalDialog() override;
  void PrimaryMainFrameWasResized(bool width_changed) override;

  base::WeakPtr<FedCmAccountSelectionView> GetWeakPtr();

 protected:
  friend class FedCmAccountSelectionViewBrowserTest;

  // Returns an AccountSelectionViewBase to render bubble dialogs for
  // widget flows, otherwise returns an AccountSelectionViewBase to render
  // modal dialogs for button flows. Registers any observers. May fail and
  // return nullptr if there is no browser or tab strip model. Virtual for
  // testing purposes.
  virtual AccountSelectionViewBase* CreateAccountSelectionView(
      const std::u16string& rp_for_display,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      bool has_modal_support);

  // Gets the type of dialog shown. Virtual for testing purposes.
  virtual DialogType GetDialogType();

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
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           UserClosingPopupAfterVerifyingSheetShouldNotify);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           AccountChooserResultMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           RequestPermissionFalseAndNewIdpDataDisclosureText);

  enum class State {
    // User is shown message that they are not currently signed-in to IdP.
    // Dialog has button to sign-in to IdP.
    IDP_SIGNIN_STATUS_MISMATCH,

    // User is shown a single account they have with IDP and is prompted to
    // select or continue with the account. On a widget flow bubble, this may
    // contain disclosure text which prompts the user to grant permission for
    // this account they have with IDP to communicate with RP.
    SINGLE_ACCOUNT_PICKER,

    // User is shown list of accounts they have with IDP and is prompted to
    // select an account.
    MULTI_ACCOUNT_PICKER,

    // User is shown the list of newly logged in accounts. Used when the user
    // logs in to an IDP.
    NEWLY_LOGGED_IN_ACCOUNT_PICKER,

    // User is prompted to grant permission for a specific account they have
    // with IDP to communicate with RP on the button flow modal.
    REQUEST_PERMISSION,

    // Shown after the user has granted permission while the id token is being
    // fetched.
    VERIFYING,

    // Shown when the user is being shown a dialog that auto re-authn is
    // happening.
    AUTO_REAUTHN,

    // Shown when an error has occurred during the user's sign-in attempt and
    // IDP has not provided any details on the failure.
    SIGN_IN_ERROR,

    // Shown after the user has triggered a button flow and while the accounts
    // are being fetched.
    LOADING,

    // Shown when we wish to display only a single returning account. Used when
    // there are multiple IDPs and exactly one returning account.
    SINGLE_RETURNING_ACCOUNT_PICKER
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
  // FedCmPopupWindowResult in
  // tools/metrics/histograms/metadata/blink/enums.xml.
  enum class PopupWindowResult {
    kAccountsReceivedAndPopupClosedByIdp,
    kAccountsReceivedAndPopupNotClosedByIdp,
    kAccountsNotReceivedAndPopupClosedByIdp,
    kAccountsNotReceivedAndPopupNotClosedByIdp,

    kMaxValue = kAccountsNotReceivedAndPopupNotClosedByIdp
  };

  // This enum describes the outcome an account chooser and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end. This enum should be kept in sync with
  // AccountChooserResult in
  // chrome/browser/ui/android/webid/AccountSelectionMediator.java as well as
  // FedCmAccountChooserResult in tools/metrics/histograms/enums.xml.
  enum class AccountChooserResult {
    kAccountRow,
    kCancelButton,
    kUseOtherAccountButton,
    kTabClosed,
    // Android-specific
    kSwipe,
    // Android-specific
    kBackPress,
    // Android-specific
    kTapScrim,

    kMaxValue = kTapScrim
  };

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AccountSelectionBubbleView::Observer:
  void OnAccountSelected(const Account& account,
                         const content::IdentityProviderData& idp_data,
                         const ui::Event& event) override;
  void OnLinkClicked(LinkType link_type,
                     const GURL& url,
                     const ui::Event& event) override;
  void OnBackButtonClicked() override;
  void OnCloseButtonClicked(const ui::Event& event) override;
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url,
                    const ui::Event& event) override;
  void OnGotIt(const ui::Event& event) override;
  void OnMoreDetails(const ui::Event& event) override;
  void OnChooseAnAccountClicked() override;

  // Returns false if `this` got deleted. In that case, the caller should not
  // access any further member variables.
  bool ShowVerifyingSheet(const Account& account,
                          const content::IdentityProviderData& idp_data);

  // Shows the dialog widget.
  void ShowDialogWidget();

  // Returns the SheetType to be used for metrics reporting.
  AccountSelectionView::SheetType GetSheetType();

  // Notify the delegate that the widget was closed with reason
  // `dismiss_reason`.
  void OnDismiss(
      content::IdentityRequestDialogController::DismissReason dismiss_reason);

  // Gets the dialog widget from the account selection view, if available.
  // Otherwise, return a nullptr.
  base::WeakPtr<views::Widget> GetDialogWidget();

  // Resets `account_selection_view_`. Typically, to recreate it later to show a
  // different kind of dialog. Virtual for testing purposes.
  virtual void MaybeResetAccountSelectionView();

  // Returns whether an IDP sign-in pop-up window is currently open.
  bool IsIdpSigninPopupOpen();

  // Returns whether the dialog widget is ready.
  bool IsDialogWidgetReady();

  // Returns whether the dialog widget should be shown.
  bool ShouldShowDialogWidget();

  // Updates the dialog's position and shows the dialog.
  void UpdateAndShowDialogWidget();

  // Hides the dialog widget and notifies the input protector.
  void HideDialogWidget();

  // Shows the multi account picker and updates the internal state.
  void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      bool show_back_button,
      bool is_choose_an_account);

  std::vector<IdentityProviderDataPtr> idp_list_;

  std::vector<IdentityRequestAccountPtr> accounts_;

  // This class needs to own the IDP display data for a newly logged in account
  // since the AccountSelectionBubbleView does not take ownership. This is a
  // vector so it is easy to pass to CreateMultipleAccountChooser in case there
  // are multiple accounts, but it is size 0 when there are no new accounts.
  std::vector<IdentityRequestAccountPtr> new_accounts_;

  std::u16string rp_for_display_;

  State state_{State::MULTI_ACCOUNT_PICKER};

  DialogType dialog_type_{DialogType::BUBBLE};

  // Whether to notify the delegate when the widget is closed.
  bool notify_delegate_of_dismiss_{true};

  std::unique_ptr<views::InputEventActivationProtector> input_protector_;

  std::unique_ptr<FedCmModalDialogView> popup_window_;

  // If dialog has been populated with accounts as a result of the IDP sign-in
  // flow but the IDP sign-in pop-up window has not been closed yet, we use this
  // callback to show the accounts dialog upon closing the IDP sign-in pop-up
  // window. This can happen when IDP sign-in status header is sent after the
  // sign-in flow is complete but the pop-up window is not closed yet e.g. user
  // is asked to verify phone number, change password, etc.
  base::OnceClosure show_accounts_dialog_callback_;

  // Because the tab that shows the accounts dialog may be invisible initially,
  // e.g. when a user opens a new tab, we'd delay showing the dialog until the
  // tab becomes visible. This callback notifies the controller when the dialog
  // is displayed to the user for the first time.
  base::OnceClosure accounts_displayed_callback_;

  // If dialog has NOT been populated with accounts yet as a result of the IDP
  // sign-in flow and the IDP sign-in pop-up window has been closed, we use this
  // boolean to let the widget know it should unhide itself when the dialog is
  // ready. This can happen when the accounts fetch has yet to finish but the
  // pop-up window has already been closed.
  bool is_modal_closed_but_accounts_fetch_pending_{false};

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

  // Whether the current dialog started as a single returning account dialog.
  // Used to determine whether the multi IDP picker needs to show a back button
  // or not.
  bool started_as_single_returning_account_{false};

  // Whether the last ShowMultiAccountPicker() is from a "Choose an account"
  // button. This is used to determine whether to show this title when coming
  // back from the single account confirmation dialog.
  bool last_multi_account_is_choose_an_account_{false};

  // Time when IdentityProvider.close() was called for metrics purposes.
  base::TimeTicks idp_close_popup_time_;

  // The current state of the IDP sign-in pop-up window, if initiated by user.
  // This is nullopt when no popup window has been opened.
  std::optional<PopupWindowResult> popup_window_state_;

  // The current state of the modal account chooser, if initiated by user. This
  // is nullopt when no modal account chooser has been opened.
  std::optional<AccountChooserResult> modal_account_chooser_state_;

  // An AccountSelectionViewBase to render bubble dialogs for widget flows,
  // otherwise returns an AccountSelectionViewBase to render modal dialogs
  // for button flows.
  raw_ptr<AccountSelectionViewBase> account_selection_view_;

  base::WeakPtrFactory<FedCmAccountSelectionView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
