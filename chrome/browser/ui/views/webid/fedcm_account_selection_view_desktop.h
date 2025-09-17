// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
#include "chrome/browser/ui/webid/identity_ui_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/input_event_activation_protector.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace tabs {
class TabInterface;
class ScopedAcceptMouseEventsWhileWindowInactive;
}  // namespace tabs

namespace webid {

class AccountSelectionViewBase;

// Provides an implementation of the AccountSelectionView interface on desktop,
// which creates the AccountSelectionBubbleView dialog to display the FedCM
// account chooser to the user.
// The lifetime of this class is conceptually scoped to the intersection of:
//  * IdentityDialogController, which represents the request from blink.
//  * tabs::TabInterface, which represents the tab in which the UI is shown.
// If either goes away, then this class should be destroyed. This class is owned
// as a unique_ptr by IdentityDialogController which ensures that the lifetime
// is scoped to that of RequestService. However, the lifetime must be
// manually scoped to the tabs::TabInterface. This is done by:
//  * Registering callbacks on tabs::TabInterface for relevant changes.
//  * If the tab goes away, Close() is called.
//  * All methods to show UI early exit if the tab no longer exists.
//
// At a high level this class has 3 states:
//   * No dialog widget exists.
//   * The dialog widget exists and is showing.
//   * The dialog widget exists and is not showing (because the tab is in
//     the background, or because the browser window is too small, etc.).
// Construction and destruction of the widget/view are simultaneous and
// synchronous. There are no other states or edge cases with regards to
// dialog widget/view existence.
//
// The purpose of this class is to show some UI to the user in response to a
// website calling navigator.credentials.get(). At most one piece of UI can be
// showing for a tab at any given point in time. When this class is created, the
// owner of this class is expected to call at least one of the Show*() methods.
// This creates the dialog widget. There are a few flows where the widget is
// destroyed and immediately recreated. But aside from those cases, if the
// widget is destroyed, then the UI flow is finished.
class FedCmAccountSelectionView : public AccountSelectionView,
                                  public FedCmModalDialogView::Observer,
                                  public content::WebContentsObserver,
                                  public PictureInPictureOcclusionObserver {
 public:
  enum class DialogType {
    // FedCM dialog inherits a bubble dialog, which is typically shown on the
    // top-right corner of the browser. The user can switch tabs and interact
    // with web contents.
    BUBBLE,

    // FedCM dialog inherits a modal dialog, which is typically shown in the
    // middle of the browser overlapping the line of death. The user can switch
    // tabs but cannot interact with web contents.
    MODAL,
  };

  FedCmAccountSelectionView(AccountSelectionView::Delegate* delegate,
                            tabs::TabInterface* tab);
  ~FedCmAccountSelectionView() override;

  // AccountSelectionView:
  bool Show(
      const content::RelyingPartyData& rp_data,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts) override;
  bool ShowFailureDialog(
      const content::RelyingPartyData& rp_data,
      const std::string& idp_etld_plus_one,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata) override;
  bool ShowErrorDialog(const content::RelyingPartyData& rp_data,
                       const std::string& idp_etld_plus_one,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override;
  bool ShowLoadingDialog(const content::RelyingPartyData& rp_data,
                         const std::string& idp_etld_plus_one,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode) override;
  bool ShowVerifyingDialog(const content::RelyingPartyData& rp_data,
                           const IdentityProviderDataPtr& idp_data,
                           const IdentityRequestAccountPtr& account,
                           Account::SignInMode sign_in_mode,
                           blink::mojom::RpMode rp_mode) override;

  void ShowUrl(LinkType link_type, const GURL& url) override;
  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;
  content::WebContents* GetRpWebContents() override;

  // FedCmModalDialogView::Observer
  // This method is only called when the user manually closes the popup window.
  // This is because the only way to programmatically close the popup is via
  // CloseModalDialog, which resets the observer of popup_window_ which prevents
  // this method from being called.
  void OnPopupWindowDestroyed() override;

  // Close always destroys the widget. If `hide_widget` is true, the contents
  // view is extracted from the widget and parked (see comment for
  // parked_dialog_view_). This view is possibly used later if the dialog is
  // recreated and shown with the same content. This is never called from a user
  // action.
  void Close(bool notify_delegate, bool hide_widget);

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  void SetInputEventActivationProtectorForTesting(
      std::unique_ptr<views::InputEventActivationProtector>);

  // AccountSelectionBubbleView::Observer:
  content::WebContents* ShowModalDialog(const GURL& url,
                                        blink::mojom::RpMode rp_mode) override;
  void CloseModalDialog() override;
  void PrimaryMainFrameWasResized(bool width_changed) override;

  base::WeakPtr<FedCmAccountSelectionView> GetWeakPtr();

  // Called when the associated tab enters the foreground.
  // Public for testing.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called after the widget associated with Show() has been shown.
  void OnAccountsDisplayed();

  // Called when a user either selects the account from the multi-account
  // chooser or clicks the "continue" button.
  // Takes `account` as well as `idp_data` since passing `account_id`
  // is insufficient in the multiple IDP case.
  // Returns whether an account is successfully selected. e.g. if a user clicks
  // the account too soon, the input protector may reject the click event, in
  // which case the account selection will not go through and `false` will be
  // returned.
  bool OnAccountSelected(const IdentityRequestAccountPtr& account,
                         const ui::Event& event);

  // Called when the user clicks "privacy policy" or "terms of service" link.
  void OnLinkClicked(
      content::IdentityRequestDialogController::LinkType link_type,
      const GURL& url,
      const ui::Event& event);

  // Called when the user clicks "back" button.
  void OnBackButtonClicked();

  // Called when the user clicks "close" button.
  void OnCloseButtonClicked(const ui::Event& event);

  // Called when the user clicks the "continue" button on the sign-in
  // failure dialog or wants to sign in to another account.
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url,
                    const ui::Event& event);

  // Called when the user clicks "got it" button.
  void OnGotIt(const ui::Event& event);

  // Called when the user clicks the "more details" button on the error
  // dialog.
  void OnMoreDetails(const ui::Event& event);

  // Called when the user clicks on the 'Choose an account' button
  void OnChooseAnAccountClicked();

  // Public for testing.
  AccountSelectionViewBase* account_selection_view() {
    return account_selection_view_.get();
  }

  // Whether the dialog can fit in the web contents at its preferred size.
  // Virtual and public for testing.
  virtual bool CanFitInWebContents();

  // Updates the position of the dialog. Used when the contents of the dialog
  // has changed or when the widget which the dialog is anchored on has been
  // resized.
  // Virtual for testing.
  virtual void UpdateDialogPosition();

  // Gets the dialog widget from the account selection view, if available.
  // Otherwise, return a nullptr.
  views::Widget* GetDialogWidget();

  // Returns whether the dialog widget exists and is visible.
  bool IsDialogWidgetVisible() const;

  // Returns whether the dialog had been created but is no longer visible and
  // may be made visible again with the same contents. Used for Testing.
  bool HasDialogContentsViewForTesting() const;

  // Called when the tab will be removed from the window.
  // Public for testing.
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  FedCmModalDialogView* GetPopupWindowForTesting();

 protected:
  friend class FedCmAccountSelectionViewBrowserTest;

  // Virtual for testing.
  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory();

  // Returns the anchor view used by the bubble. Virtual for testing.
  virtual views::View* GetAnchorView();

  // Creates the appropriate dialog view, depending on whether the
  // dialog is bubble or modal. `out_dialog_type` is an output parameter.
  // Virtual for testing.
  virtual AccountSelectionViewBase* CreateDialogView(
      bool has_modal_support,
      const content::RelyingPartyData& rp_data,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      DialogType* out_dialog_type);

  // Creates the appropriate dialog widget, depending on whether the
  // dialog is bubble or modal.
  // Virtual for testing.
  virtual std::unique_ptr<views::Widget> CreateDialogWidget();
  // Remove the content view from the dialog/bubble and return ownership.
  // virtual for testing.
  virtual std::unique_ptr<views::View> ExtractDialogContentsView();

  // Creates a popup window that is used to sign in to the IdP, or other flows.
  // Virtual for testing.
  virtual std::unique_ptr<FedCmModalDialogView> CreatePopupWindow();

 protected:
  virtual void ShowDialog(
      views::Widget* widget,
      std::unique_ptr<tabs::TabDialogManager::Params> params);
  virtual void UpdateDialogVisibility(bool requested_visibility);
  virtual bool IsDialogManaged(views::Widget* widget);

  // This contains the "parked" contents of the dialog_widget_. It is placed
  // here prior to the dialog_widget_ being constructed or when the
  // dialog_widget_ is destroyed when it is hidden. The dialog_widget_ will be
  // recreated on-demand and this view handed off to the new instance. It is
  // protected for testing.
  std::unique_ptr<views::View> parked_dialog_view_;

 private:
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewBrowserTest,
                           ModalDialogThenShowThenCloseModalDialog);
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
                           LoadingDialogResultMetric);
  FRIEND_TEST_ALL_PREFIXES(FedCmAccountSelectionViewDesktopTest,
                           DisclosureDialogResultMetric);
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
  };

  // This enum describes the outcome of the mismatch dialog and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end.
  // LINT.IfChange(MismatchDialogResult)

  enum class MismatchDialogResult {
    kContinued = 0,
    kDismissedByCloseIcon = 1,
    kDismissedForOtherReasons = 2,
    kMaxValue = kDismissedForOtherReasons
  };

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmMismatchDialogResult)

  // This enum describes the outcome of the pop-up window and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end.
  // LINT.IfChange(PopupWindowResult)

  enum class PopupWindowResult {
    kAccountsReceivedAndPopupClosedByIdp = 0,
    kAccountsReceivedAndPopupNotClosedByIdp = 1,
    kAccountsNotReceivedAndPopupClosedByIdp = 2,
    kAccountsNotReceivedAndPopupNotClosedByIdp = 3,
    kMaxValue = kAccountsNotReceivedAndPopupNotClosedByIdp
  };

  // LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:FedCmPopupWindowResult)

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Called when the tab's modalUI is shown or hidden.
  void ModalUIChanged(tabs::TabInterface* tab);

  // Returns false if `this` got deleted. In that case, the caller must early
  // return.
  bool NotifyDelegateOfAccountSelection(
      const Account& account,
      const content::IdentityProviderData& idp_data);

  // Shows the verifying sheet.
  void ShowVerifyingSheet(const IdentityRequestAccountPtr& account);

  // Shows the dialog widget.
  void ShowDialogWidget();

  // Returns the SheetType to be used for metrics reporting.
  webid::SheetType GetSheetType();

  // Returns whether an IDP sign-in pop-up window is currently open.
  bool IsIdpSigninPopupOpen();

  // Hides the dialog widget and notifies the input protector.
  void HideDialogWidget();

  // Shows the multi account picker and updates the internal state.
  void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const gfx::Image& rp_icon,
      bool show_back_button);

  // PictureInPictureOcclusionObserver:
  void OnOcclusionStateChanged(bool occluded) override;

  // This should be called exactly once when the dialog is dismissed.
  using DismissReason = content::IdentityRequestDialogController::DismissReason;
  void LogDialogDismissal(DismissReason dismiss_reason);

  // Creates account_selection_view_ (different subclasses for
  // bubble/modal) and dialog_widget_ if it hasn't been created yet.
  // Otherwise, updates the view's title if any data has changed.
  void CreateOrUpdateViewAndWidget(
      const content::RelyingPartyData& rp_data,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      bool has_modal_support);

  // Synchronously closes dialog_widget_. This method can result in synchronous
  // destruction of `this`.
  void CloseWidget(bool notify_delegate,
                   views::Widget::ClosedReason reason,
                   bool hide_widget);

  // Called when the user closes the dialog. This is called by
  // OnCloseButtonClicked() if the user clicks the close button, and directly
  // called by views toolkit if the user dismisses with another mechanism such
  // as pressing "esc".
  void OnUserClosedDialog(views::Widget::ClosedReason reason);

  // This method calculates whether the dialog should be visible. This method
  // always updates the dialog position if the dialog should be visible. If the
  // dialog should be visible, and it is not, this method makes the dialog
  // visible and focuses the dialog.
  // All control flows that want to show the dialog must go through this method.
  // This ensures the complex logic to determine visibility is centralized.
  void UpdateDialogVisibilityAndPosition();

  // Called when any of the Show*() methods is called.
  void ResetDialogWidgetStateOnAnyShow();

  // Returns the intended bounds and position of the dialog. Rather than
  // directly setting the bounds, the bounds are instead returned since the
  // dialog manager may decide to animate the bounds if they happen to change.
  gfx::Rect GetDialogBounds();

  // Set |should_show| to false if the dialog should be hidden. Since
  // |should_show| is initially set by the caller, this function should only set
  // it to false if it determines the dialog should be hidden for whatever
  // reason. The caller *may* have decided to not show the dialog by setting the
  // initial value to false.
  void ShouldShowDialog(bool& should_show);

  std::vector<IdentityProviderDataPtr> idp_list_;

  std::vector<IdentityRequestAccountPtr> accounts_;

  // This class needs to own the IDP display data for a newly logged in account
  // since the AccountSelectionBubbleView does not take ownership. This is a
  // vector so it is easy to pass to CreateMultipleAccountChooser in case there
  // are multiple accounts, but it is size 0 when there are no new accounts.
  std::vector<IdentityRequestAccountPtr> new_accounts_;

  // The RP icon to be displayed in the UI when needed.
  gfx::Image rp_icon_;

  State state_{State::MULTI_ACCOUNT_PICKER};

  DialogType dialog_type_{DialogType::BUBBLE};

  std::unique_ptr<views::InputEventActivationProtector> input_protector_;

  std::unique_ptr<FedCmModalDialogView> popup_window_;

  // If dialog has been populated with accounts as a result of the IDP sign-in
  // flow but the IDP sign-in pop-up window has not been closed yet, we use this
  // callback to show the accounts dialog upon closing the IDP sign-in pop-up
  // window. This can happen when IDP sign-in status header is sent after the
  // sign-in flow is complete but the pop-up window is not closed yet e.g. user
  // is asked to verify phone number, change password, etc.
  base::OnceClosure show_accounts_dialog_callback_;

  // When we are using the bubble dialog, and the user opens an IDP login popup
  // window, and a new accounts fetch has not yet completed, we keep hiding the
  // dialog widget ever after CloseModalDialog(). This ensures we do not show
  // the previous UI (could be mismatch UI or account picker). Instead we wait
  // for Show() to be called with new information, and then show the updated
  // dialog widget.
  //
  // If the IDP never sends a login status header, then CloseModalDialog() will
  // closes the popup, but Show() will never be called. This is currently not
  // handled by the fedcm control-flow. For now we keep hiding the dialog.
  bool hide_dialog_widget_after_idp_login_popup_{false};

  // If Show() is called, the intention is to show the accounts dialog. This
  // callback is invoked when the widget is actually shown for the first time.
  base::OnceClosure accounts_widget_shown_callback_;

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
  // This is nullopt when no popup window has been opened.
  std::optional<PopupWindowResult> popup_window_state_;

  // The current state of the modal account chooser, if initiated by user. This
  // is nullopt when no modal account chooser has been opened.
  std::optional<webid::AccountChooserResult> modal_account_chooser_state_;

  // The current state of the modal loading dialog. This is nullopt when no
  // modal loading dialog has been opened.
  std::optional<webid::LoadingDialogResult> modal_loading_dialog_state_;

  // The current state of the modal disclosure dialog. This is nullopt when no
  // modal disclosure dialog has been opened.
  std::optional<webid::DisclosureDialogResult> modal_disclosure_dialog_state_;

  // Whether the widget is occluded by PIP (and therefore we should ignore
  // inputs).
  bool is_occluded_by_pip_{false};

  // Observer for widget occlusion.
  std::unique_ptr<ScopedPictureInPictureOcclusionObservation>
      pip_occlusion_observation_;

  // The tab hosting the associated UI.
  // This class is owned by IdentityDialogController and thus can outlive the
  // associated UI. Any uses of tab_ must be preceded by a nullptr check.
  raw_ptr<tabs::TabInterface> tab_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Disable events to the tab contents if and only if the modal widget is
  // showing.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
  // Allows the tab to receive mouse events even when the window is inactive
  // (because the bubble window is active). This is if and only if the bubble
  // widget is showing.
  std::unique_ptr<tabs::ScopedAcceptMouseEventsWhileWindowInactive>
      tab_accept_mouse_events_;

  // This is the delegate used by the dialog_widget_. It is owned by this class
  // and is constructed/destroyed as needed to create the dialog_widget_.
  std::unique_ptr<views::WidgetDelegate> widget_delegate_;

  // Widget that owns the view.
  std::unique_ptr<views::Widget> dialog_widget_;

  // This controls the contents of the dialog_widget_. Conceptually there
  // is a view if and only if there is a widget. The two are constructed
  // together and destroyed together. `dialog_widget_` owns
  // `account_selection_view_` as a View attached to the root of the Widget.
  raw_ptr<AccountSelectionViewBase> account_selection_view_ = nullptr;

  base::WeakPtrFactory<FedCmAccountSelectionView> weak_ptr_factory_{this};
};

}  // namespace webid

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FEDCM_ACCOUNT_SELECTION_VIEW_DESKTOP_H_
