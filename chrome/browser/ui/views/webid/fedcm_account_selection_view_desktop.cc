// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/inactive_window_mouse_event_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/extensions/security_dialog_tracker.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/ui/webid/identity_ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

// static
int AccountSelectionView::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  // TODO(crbug.com/348673144): Decide whether to keep circle cropping IDP
  // icons.
  return (rp_mode == blink::mojom::RpMode::kActive
              ? webid::kModalIdpIconSize
              : webid::kBubbleIdpIconSize) /
         webid::kMaskableWebIconSafeZoneRatio;
}

// static
int AccountSelectionView::GetBrandIconIdealSize(blink::mojom::RpMode rp_mode) {
  // As only a single brand icon is selected and the user can have monitors with
  // different screen densities, make the ideal size be the size which works
  // with a high density display (if the OS supports high density displays).
  const float max_supported_scale =
      ui::GetScaleForMaxSupportedResourceScaleFactor();
  return round(GetBrandIconMinimumSize(rp_mode) * max_supported_scale);
}

namespace webid {

using DismissReason = content::IdentityRequestDialogController::DismissReason;

FedCmAccountSelectionView::FedCmAccountSelectionView(
    AccountSelectionView::Delegate* delegate,
    tabs::TabInterface* tab)
    : AccountSelectionView(delegate),
      content::WebContentsObserver(delegate->GetWebContents()),
      tab_(tab) {
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&FedCmAccountSelectionView::TabForegrounded,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&FedCmAccountSelectionView::TabWillEnterBackground,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&FedCmAccountSelectionView::WillDiscardContents,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &FedCmAccountSelectionView::WillDetach, weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterModalUIChanged(
      base::BindRepeating(&FedCmAccountSelectionView::ModalUIChanged,
                          weak_ptr_factory_.GetWeakPtr())));
}

FedCmAccountSelectionView::~FedCmAccountSelectionView() {
  Close(/*notify_delegate=*/false);
}

void FedCmAccountSelectionView::ShowDialogWidget() {
  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  input_protector_->VisibilityChanged(true);
  GetDialogWidget()->Show();
  if (dialog_type_ == DialogType::MODAL) {
    scoped_ignore_input_events_ =
        web_contents()->IgnoreInputEvents(std::nullopt);
  } else {
    if (tab_) {
      if (tabs::TabFeatures* features = tab_->GetTabFeatures()) {
        if (tabs::
                InactiveWindowMouseEventController* inactive_event_controller =
                    features->inactive_window_mouse_event_controller()) {
          tab_accept_mouse_events_ =
              inactive_event_controller->AcceptMouseEventsWhileWindowInactive();
        }
      }
    }
  }

  if (accounts_widget_shown_callback_) {
    std::move(accounts_widget_shown_callback_).Run();
  }
}

bool FedCmAccountSelectionView::Show(
    const content::RelyingPartyData& rp_data,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    blink::mojom::RpMode rp_mode,
    const std::vector<IdentityRequestAccountPtr>& new_accounts) {
  if (!tab_) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // If IDP sign-in pop-up is open, we delay the showing of the accounts dialog
  // until the pop-up is destroyed.
  if (IsIdpSigninPopupOpen()) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupNotClosedByIdp;
    // We need to use ShowNoReturn() here because it is not allowed to bind
    // WeakPtrs to methods with return values.
    show_accounts_dialog_callback_ =
        base::BindOnce(base::IgnoreResult(&FedCmAccountSelectionView::Show),
                       weak_ptr_factory_.GetWeakPtr(), rp_data, idp_list,
                       accounts, rp_mode, new_accounts);
    // This is considered successful since we are intentionally delaying showing
    // the UI.
    return true;
  }

  ResetDialogWidgetStateOnAnyShow();
  accounts_widget_shown_callback_ =
      base::BindOnce(&FedCmAccountSelectionView::OnAccountsDisplayed,
                     weak_ptr_factory_.GetWeakPtr());

  // TODO(crbug.com/41491333): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = true;

  idp_list_ = idp_list;
  accounts_ = accounts;
  new_accounts_ = new_accounts;
  rp_icon_ = rp_data.rp_icon;

  size_t accounts_or_mismatches_size = accounts.size();
  blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn;
  for (const auto& identity_provider : idp_list) {
    // If `identity_provider` has a login status mismatch, we show the login
    // button for it. In this case, there should be no accounts from that
    // provider.
    if (identity_provider->has_login_status_mismatch) {
      ++accounts_or_mismatches_size;
    }

    // TODO(crbug.com/40252518): Decide what we should display if the IdPs use
    // different contexts here.
    rp_context = identity_provider->rp_context;
  }

  bool has_filtered_out_accounts = false;
  for (const auto& account : accounts) {
    if (account->is_filtered_out) {
      has_filtered_out_accounts = true;
      break;
    }
  }

  std::optional<std::u16string> idp_title =
      idp_list_.size() == 1u
          ? std::make_optional<std::u16string>(
                base::UTF8ToUTF16(idp_list_[0]->idp_for_display))
          : std::nullopt;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead. We also reset for widget multi IDP to recalculate the title
  // and other parts of the header.
  if ((rp_mode == blink::mojom::RpMode::kPassive && idp_list_.size() > 1) ||
      (rp_mode == blink::mojom::RpMode::kActive && !has_modal_support)) {
    Close(/*notify_delegate=*/false);
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    CreateViewAndWidget(rp_data, idp_title, rp_context, rp_mode,
                        has_modal_support);
  }

  if (!new_accounts.empty()) {
    // When we just logged in to an account that   not a single returning
    // account: on the modal, we'd show all the accounts and on the bubble, we'd
    // show only the new accounts.
    const content::IdentityProviderData& new_idp_data =
        *new_accounts_[0]->identity_provider;

    if (dialog_type_ == DialogType::MODAL) {
      // The browser trusted login state controls whether we'd skip the next
      // dialog. One caveat: if a user was logged out of the IdP and they just
      // logged in with a returning account from the LOADING state, we do not
      // skip the next UI when mediation mode is `required` because there was
      // not user mediation acquired yet in this case.
      bool should_show_verifying_sheet =
          new_accounts_[0]->browser_trusted_login_state ==
              Account::LoginState::kSignIn &&
          state_ != State::LOADING;
      // The IDP claimed login state controls whether we show disclosure text,
      // if we do not skip the next dialog. Also skip when
      // `disclosure_fields` is empty (controlled by the fields API).
      bool should_show_request_permission_dialog =
          new_accounts_[0]->login_state != Account::LoginState::kSignIn &&
          !new_idp_data.disclosure_fields.empty();

      if (should_show_verifying_sheet) {
        state_ = State::VERIFYING;
        if (!NotifyDelegateOfAccountSelection(*new_accounts_[0],
                                              new_idp_data)) {
          // `this` has been deleted.
          return false;
        }
        ShowVerifyingSheet(new_accounts_[0]);
      } else if (should_show_request_permission_dialog) {
        state_ = State::REQUEST_PERMISSION;
        account_selection_view_->ShowRequestPermissionDialog(new_accounts_[0]);
        // This is a placeholder assuming the tab containing the disclosure
        // dialog will be closed. This will be updated upon clicking on
        // continue, back or cancel button. If none of these buttons are clicked
        // by time the dialog is closed, it means our placeholder assumption is
        // true i.e. the user has closed the tab.
        modal_disclosure_dialog_state_ =
            webid::DisclosureDialogResult::kDestroy;
      } else {
        // Normally we'd show the request permission dialog but without the
        // disclosure text, there is no material difference between the account
        // picker and the request permission dialog. We show the account picker
        // with most recently signed in accounts at the top to reduce the
        // exposure of extra UI surfaces and to work around the account picker
        // not having a back button.
        ShowMultiAccountPicker(accounts_, idp_list_, rp_icon_,
                               /*show_back_button=*/false);
      }
    } else {
      if (new_accounts_.size() == 1u) {
        state_ = State::SINGLE_ACCOUNT_PICKER;
        bool supports_add_account =
            rp_mode == blink::mojom::RpMode::kActive &&
            new_accounts_[0]
                ->identity_provider->idp_metadata.supports_add_account;
        account_selection_view_->ShowSingleAccountConfirmDialog(
            new_accounts_[0],
            /*show_back_button=*/accounts_or_mismatches_size > 1u ||
                supports_add_account);
      } else {
        ShowMultiAccountPicker(
            new_accounts_, {new_accounts_[0]->identity_provider}, rp_icon_,
            /*show_back_button=*/accounts_or_mismatches_size >
                new_accounts_.size());
        // Override the state to NEWLY_LOGGED_IN_ACCOUNT_PICKER so the back
        // button works correctly.
        state_ = State::NEWLY_LOGGED_IN_ACCOUNT_PICKER;
      }
    }
  } else if (idp_list_.size() == 1u && accounts_or_mismatches_size == 1u) {
    if (dialog_type_ == DialogType::BUBBLE && has_filtered_out_accounts) {
      // The logic to support add account is in ShowMultiAccountPicker for the
      // bubble dialog.
      ShowMultiAccountPicker(accounts_, idp_list_, rp_icon_,
                             /*show_back_button=*/false);
    } else {
      state_ = State::SINGLE_ACCOUNT_PICKER;
      account_selection_view_->ShowSingleAccountConfirmDialog(
          accounts_[0],
          /*show_back_button=*/false);
    }
  } else {
    ShowMultiAccountPicker(accounts_, idp_list_, rp_icon_,
                           /*show_back_button=*/false);
  }
  UpdateDialogVisibilityAndPosition();

  if (!idp_close_popup_time_.is_null()) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupClosedByIdp;
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "Blink.FedCm.IdpSigninStatus."
        "IdpClosePopupToBrowserShowAccountsDuration",
        base::TimeTicks::Now() - idp_close_popup_time_);
  }

  if (dialog_type_ == DialogType::MODAL &&
      (state_ == State::SINGLE_ACCOUNT_PICKER ||
       state_ == State::MULTI_ACCOUNT_PICKER)) {
    // This is a placeholder assuming the tab containing the account chooser
    // will be closed. This will be updated upon user action i.e. clicking on
    // account row, cancel button or use other account button. If we do not
    // receive any of these actions by time the dialog is closed, it means our
    // placeholder assumption is true i.e. the user has closed the tab.
    modal_account_chooser_state_ = webid::AccountChooserResult::kTabClosed;
  }

  if (modal_loading_dialog_state_ &&
      modal_loading_dialog_state_ !=
          webid::LoadingDialogResult::kProceedThroughPopup) {
    modal_loading_dialog_state_ = webid::LoadingDialogResult::kProceed;
  }

  return true;
}

bool FedCmAccountSelectionView::ShowFailureDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata) {
  if (!tab_) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  state_ = State::IDP_SIGNIN_STATUS_MISMATCH;
  ResetDialogWidgetStateOnAnyShow();

  // TODO(crbug.com/41491333): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = false;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead.  We also reset for widget multi IDP to recalculate the
  // title and other parts of the header.
  if ((rp_mode == blink::mojom::RpMode::kPassive && idp_list_.size() > 1) ||
      (rp_mode == blink::mojom::RpMode::kActive && !has_modal_support)) {
    Close(/*notify_delegate=*/false);
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    CreateViewAndWidget(rp_data, base::UTF8ToUTF16(idp_etld_plus_one),
                        rp_context, rp_mode, has_modal_support);
  }

  account_selection_view_->ShowFailureDialog(
      base::UTF8ToUTF16(idp_etld_plus_one), idp_metadata);
  UpdateDialogVisibilityAndPosition();
  return true;
}

bool FedCmAccountSelectionView::ShowErrorDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  if (!tab_) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  state_ = State::SIGN_IN_ERROR;
  ResetDialogWidgetStateOnAnyShow();

  bool has_modal_support = true;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead. We also reset for widget multi IDP to recalculate the title
  // and other parts of the header.
  if ((rp_mode == blink::mojom::RpMode::kPassive && idp_list_.size() > 1) ||
      (rp_mode == blink::mojom::RpMode::kActive && !has_modal_support)) {
    Close(/*notify_delegate=*/false);
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    CreateViewAndWidget(rp_data, base::UTF8ToUTF16(idp_etld_plus_one),
                        rp_context, rp_mode, has_modal_support);
  }

  account_selection_view_->ShowErrorDialog(base::UTF8ToUTF16(idp_etld_plus_one),
                                           idp_metadata, error);
  UpdateDialogVisibilityAndPosition();
  return true;
}

bool FedCmAccountSelectionView::ShowLoadingDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode) {
  if (!tab_) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  CHECK(rp_mode == blink::mojom::RpMode::kActive);

  state_ = State::LOADING;
  ResetDialogWidgetStateOnAnyShow();

  bool create_view = !account_selection_view_;
  if (create_view) {
    CreateViewAndWidget(rp_data, base::UTF8ToUTF16(idp_etld_plus_one),
                        rp_context, rp_mode,
                        /*has_modal_support=*/true);
  }

  UpdateDialogVisibilityAndPosition();
  modal_loading_dialog_state_ = webid::LoadingDialogResult::kDestroy;
  return true;
}

bool FedCmAccountSelectionView::ShowVerifyingDialog(
    const content::RelyingPartyData& rp_data,
    const IdentityProviderDataPtr& idp_data,
    const IdentityRequestAccountPtr& account,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode) {
  if (!tab_) {
    return false;
  }

  // If IDP sign-in pop-up is open, we delay the showing of the accounts dialog
  // until the pop-up is destroyed.
  // TODO(crbug.com/419535307): move the control logic to the backend.
  if (IsIdpSigninPopupOpen()) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupNotClosedByIdp;
    // We need to use base::IgnoreResult here because it is not allowed to bind
    // WeakPtrs to methods with return values.
    show_accounts_dialog_callback_ = base::BindOnce(
        base::IgnoreResult(&FedCmAccountSelectionView::ShowVerifyingDialog),
        weak_ptr_factory_.GetWeakPtr(), rp_data, idp_data, account,
        sign_in_mode, rp_mode);
    // This is considered successful since we are intentionally delaying showing
    // the UI.
    return true;
  }

  ResetDialogWidgetStateOnAnyShow();
  accounts_widget_shown_callback_ =
      base::BindOnce(&FedCmAccountSelectionView::OnAccountsDisplayed,
                     weak_ptr_factory_.GetWeakPtr());

  bool create_view = !account_selection_view_;
  if (create_view) {
    // While the verifying UI may not need to show RP and IdP data in case of
    // auto reauthn, we need them anyway to prepare for potential error UI
    // afterwards.
    CreateViewAndWidget(rp_data, base::UTF8ToUTF16(idp_data->idp_for_display),
                        idp_data->rp_context, rp_mode,
                        /*has_modal_support=*/true);
  }

  if (sign_in_mode == Account::SignInMode::kAuto) {
    state_ = State::AUTO_REAUTHN;
  }

  // Auto re-authn in active mode does not update the loading UI.
  if (dialog_type_ == DialogType::MODAL) {
    modal_loading_dialog_state_ = webid::LoadingDialogResult::kProceed;
    return false;
  }

  ShowVerifyingSheet(account);
  UpdateDialogVisibilityAndPosition();

  return true;
}

void FedCmAccountSelectionView::ShowUrl(LinkType link_type, const GURL& url) {
  Browser* browser = chrome::FindBrowserWithTab(delegate_->GetWebContents());
  TabStripModel* tab_strip_model = browser->tab_strip_model();

  DCHECK(tab_strip_model);
  // Add a tab for the URL at the end of the tab strip, in the foreground.
  tab_strip_model->delegate()->AddTabAt(url, -1, true);

  switch (link_type) {
    case LinkType::TERMS_OF_SERVICE:
      UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.SignUp.TermsOfServiceClicked", true);
      break;

    case LinkType::PRIVACY_POLICY:
      UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.SignUp.PrivacyPolicyClicked", true);
      break;
  }
}

std::string FedCmAccountSelectionView::GetTitle() const {
  return account_selection_view_->GetDialogTitle();
}

std::optional<std::string> FedCmAccountSelectionView::GetSubtitle() const {
  return account_selection_view_->GetDialogSubtitle();
}

void FedCmAccountSelectionView::PrimaryPageChanged(content::Page& page) {
  // Close the dialog when the user navigates within the same tab.
  Close(/*notify_delegate=*/true);
}

void FedCmAccountSelectionView::SetInputEventActivationProtectorForTesting(
    std::unique_ptr<views::InputEventActivationProtector> input_protector) {
  input_protector_ = std::move(input_protector);
}

void FedCmAccountSelectionView::CreateViewAndWidget(
    const content::RelyingPartyData& rp_data,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    bool has_modal_support) {
  CHECK(!dialog_widget_);
  CHECK(tab_);
  account_selection_view_ =
      CreateDialogView(has_modal_support, rp_data, idp_title, rp_context,
                       rp_mode, &dialog_type_);
  dialog_widget_ = CreateDialogWidget();
  dialog_widget_->MakeCloseSynchronous(base::BindOnce(
      &FedCmAccountSelectionView::OnUserClosedDialog, base::Unretained(this)));
}

void FedCmAccountSelectionView::OnAccountsDisplayed() {
  delegate_->OnAccountsDisplayed();
}

void FedCmAccountSelectionView::OnAccountSelected(
    const IdentityRequestAccountPtr& account,
    const ui::Event& event) {
  DCHECK(state_ != State::IDP_SIGNIN_STATUS_MISMATCH);
  DCHECK(state_ != State::AUTO_REAUTHN);

  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      is_occluded_by_pip_) {
    return;
  }

  if (modal_account_chooser_state_) {
    modal_account_chooser_state_ = webid::AccountChooserResult::kAccountRow;
  }

  if (modal_disclosure_dialog_state_) {
    modal_disclosure_dialog_state_ = webid::DisclosureDialogResult::kContinue;
  }

  const content::IdentityProviderData& idp_data = *account->identity_provider;
  // If the account is a returning user or if the account is selected from UI
  // which shows the disclosure text or if the dialog doesn't need to ask for
  // the user's permission to share their id/email/name/picture, show the
  // verifying sheet.
  if (account->login_state != Account::LoginState::kSignUp ||
      state_ == State::REQUEST_PERMISSION ||
      (state_ == State::SINGLE_ACCOUNT_PICKER &&
       dialog_type_ == DialogType::BUBBLE) ||
      idp_data.disclosure_fields.empty()) {
    state_ = State::VERIFYING;
    if (!NotifyDelegateOfAccountSelection(*account, idp_data)) {
      // `this` was deleted.
      return;
    }
    // TODO(crbug.com/418214600): hand the control to show verifying UI over to
    // the backend.
    ShowVerifyingSheet(account);
    UpdateDialogPosition();
    return;
  }

  // At this point, the account is a non-returning user. If the dialog is modal,
  // we'd request permission through the request permission dialog.
  if (dialog_type_ == DialogType::MODAL) {
    state_ = State::REQUEST_PERMISSION;
    account_selection_view_->ShowRequestPermissionDialog(account);
    // This is a placeholder assuming the tab containing the disclosure dialog
    // will be closed. This will be updated upon proceeding to the verifying
    // sheet, clicking the back button or clicking the cancel button. If none of
    // these happen by time the dialog is closed, it means our placeholder
    // assumption is true i.e. the user has closed the tab.
    modal_disclosure_dialog_state_ = webid::DisclosureDialogResult::kDestroy;
    UpdateDialogPosition();
    return;
  }

  // At this point, the account is a non-returning user, the dialog is a bubble
  // and it is a multi account picker, there is no disclosure text on the dialog
  // so we'd request permission through a single account dialog.
  state_ = State::SINGLE_ACCOUNT_PICKER;
  account_selection_view_->ShowSingleAccountConfirmDialog(
      account, /*show_back_button=*/true);
  UpdateDialogPosition();
}

void FedCmAccountSelectionView::OnLinkClicked(LinkType link_type,
                                              const GURL& url,
                                              const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      is_occluded_by_pip_) {
    return;
  }
  ShowUrl(link_type, url);
}

void FedCmAccountSelectionView::OnBackButtonClicked() {
  // No need to protect input here since back cannot be the first event.
  if (state_ == State::REQUEST_PERMISSION) {
    modal_disclosure_dialog_state_ = webid::DisclosureDialogResult::kBack;
  }

  // If the dialog type is modal and there is only one IDP and one account, show
  // the single account picker.
  if (dialog_type_ == DialogType::MODAL && idp_list_.size() == 1u &&
      accounts_.size() == 1u) {
    state_ = State::SINGLE_ACCOUNT_PICKER;
    account_selection_view_->ShowSingleAccountConfirmDialog(
        accounts_[0], /*show_back_button=*/false);
    UpdateDialogPosition();
    return;
  }
  ShowMultiAccountPicker(accounts_, idp_list_, rp_icon_,
                         /*show_back_button=*/false);
  UpdateDialogPosition();
}

void FedCmAccountSelectionView::OnCloseButtonClicked(const ui::Event& event) {
  // Because the close button is a safe button to click and may be visible
  // even when the widget is (partially) occluded, we do not check
  // `is_occluded_by_pip_` here.
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  if (dialog_type_ == DialogType::BUBBLE) {
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.CloseVerifySheet.Desktop",
                          state_ == State::VERIFYING);

    // Record the sheet type that the user was closing.
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.ClosedSheetType.Desktop",
                              GetSheetType(), webid::SheetType::COUNT);
  }

  // Check that state_ at the time of closing is an account chooser, otherwise,
  // closing other dialogs can override the modal_account_chooser_state_.
  if (modal_account_chooser_state_ && (state_ == State::SINGLE_ACCOUNT_PICKER ||
                                       state_ == State::MULTI_ACCOUNT_PICKER)) {
    modal_account_chooser_state_ = webid::AccountChooserResult::kCancelButton;
  }

  if (modal_disclosure_dialog_state_ && state_ == State::REQUEST_PERMISSION) {
    modal_disclosure_dialog_state_ = webid::DisclosureDialogResult::kCancel;
  }

  if (state_ == State::LOADING) {
    modal_loading_dialog_state_ = webid::LoadingDialogResult::kCancel;
  }

  OnUserClosedDialog(views::Widget::ClosedReason::kCloseButtonClicked);
}

void FedCmAccountSelectionView::OnLoginToIdP(const GURL& idp_config_url,
                                             const GURL& idp_login_url,
                                             const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      is_occluded_by_pip_) {
    return;
  }

  delegate_->OnLoginToIdP(idp_config_url, idp_login_url);

  if (state_ == State::IDP_SIGNIN_STATUS_MISMATCH) {
    is_mismatch_continue_clicked_ = true;
    popup_window_state_ =
        PopupWindowResult::kAccountsNotReceivedAndPopupNotClosedByIdp;
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
        MismatchDialogResult::kContinued);
  }

  if (modal_account_chooser_state_) {
    modal_account_chooser_state_ =
        webid::AccountChooserResult::kUseOtherAccountButton;
  }
}

void FedCmAccountSelectionView::OnGotIt(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      is_occluded_by_pip_) {
    return;
  }

  delegate_->OnDismiss(DismissReason::kGotItButton);
}

void FedCmAccountSelectionView::OnMoreDetails(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      is_occluded_by_pip_) {
    return;
  }

  delegate_->OnMoreDetails();
  delegate_->OnDismiss(DismissReason::kMoreDetailsButton);
}

content::WebContents* FedCmAccountSelectionView::ShowModalDialog(
    const GURL& url,
    blink::mojom::RpMode rp_mode) {
  if (popup_window_) {
    // TODO(crbug.com/324052630): Support add account with multi IDP API. An add
    // account pop-up of a different IDP might be open, so this might need to
    // load the new IDP's login URL.
    popup_window_->ResizeAndFocusPopupWindow();
  } else {
    popup_window_ = CreatePopupWindow();
  }

  // Position the pop-up window such that the top of the pop-up window lines up
  // with the top of the active mode loading modal. This helps cover the loading
  // modal and direct user attention to the pop-up window. Note that this change
  // does not apply to other pop-up windows such as use other account, instead
  // they will be shown centred.
  if (rp_mode == blink::mojom::RpMode::kActive) {
    popup_window_->SetActiveModeSheetType(GetSheetType());
    if (state_ == State::LOADING) {
      popup_window_->SetCustomYPosition(web_contents()->GetViewBounds().y());
    }
  }

  // The modal should not be hidden when the pop-up window is displayed for
  // better UX.
  UpdateDialogVisibilityAndPosition();

  // The FedCM dialog should not be dismissed if the use other account pop-up is
  // closed, which can only be triggered from account selection. On the other
  // hand, if the popup is from another flow, then closing the popup should also
  // exit out of the entire FedCM flow.
  bool user_close_cancels_flow =
      GetSheetType() != webid::SheetType::ACCOUNT_SELECTION;
  return popup_window_->ShowPopupWindow(url, user_close_cancels_flow);
}

void FedCmAccountSelectionView::CloseModalDialog() {
  auto show_accounts_callback = std::move(show_accounts_dialog_callback_);
  if (popup_window_) {
    // If the pop-up window is for IDP sign-in (as triggered from the mismatch
    // dialog or the add account button from the account chooser), we do not
    // destroy the bubble widget and wait for the accounts fetch before
    // displaying a dialog.
    // Otherwise if the pop-up window is for AuthZ or error, we destroy the
    // bubble widget and any incoming accounts fetches would not display any
    // dialog.
    // TODO(crbug.com/40281136): Verify if the current behaviour is what we want
    // for AuthZ/error.
    if (IsIdpSigninPopupOpen()) {
      idp_close_popup_time_ = base::TimeTicks::Now();
      hide_dialog_widget_after_idp_login_popup_ = true;
      popup_window_state_ =
          PopupWindowResult::kAccountsNotReceivedAndPopupClosedByIdp;
    }

    // By resetting the observer we prevent a call to OnPopupWindowDestroyed.
    // This ensures that OnPopupWindowDestroyed is only called when the user
    // closes the popup.
    popup_window_->ResetObserver();
    popup_window_->ClosePopupWindow();
    popup_window_.reset();

    UpdateDialogVisibilityAndPosition();
  }

  if (state_ == State::LOADING) {
    modal_loading_dialog_state_ =
        webid::LoadingDialogResult::kProceedThroughPopup;
  }

  if (show_accounts_callback) {
    std::move(show_accounts_callback).Run();
    // `this` might be deleted now, do not access member variables
    // after this point.
  }
}

content::WebContents* FedCmAccountSelectionView::GetRpWebContents() {
  // This function is only used on Android.
  NOTREACHED();
}

bool FedCmAccountSelectionView::CanFitInWebContents() {
  CHECK(web_contents() && dialog_widget_);

  gfx::Size web_contents_size = web_contents()->GetSize();
  gfx::Size preferred_bubble_size =
      dialog_widget_->GetContentsView()->GetPreferredSize();

  // TODO(crbug.com/340368623): Figure out what to do when button flow modal
  // cannot fit in web contents. The offsets kRightMargin and kTopMargin pertain
  // to the bubble widget.
  return preferred_bubble_size.width() <
             (web_contents_size.width() - kRightMargin) &&
         preferred_bubble_size.height() <
             (web_contents_size.height() - kTopMargin);
}

void FedCmAccountSelectionView::UpdateDialogPosition() {
  if (dialog_type_ == DialogType::BUBBLE) {
    auto* bubble =
        static_cast<AccountSelectionBubbleView*>(account_selection_view_);
    GetDialogWidget()->SetBounds(bubble->GetBubbleBounds());
  } else {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetDialogWidget(),
        web_modal::WebContentsModalDialogManager::FromWebContents(
            web_contents())
            ->delegate()
            ->GetWebContentsModalDialogHost());
  }
}

void FedCmAccountSelectionView::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // The lifetime of FedCmAccountSelectionView is (indirectly) scoped to the
  // lifetime of the WebContents. If the WebContents will be destroyed, then
  // FedCmAccountSelectionView will eventually be destroyed as well. Clear the
  // tab and subscription to avoid doing unnecessary work.
  tab_ = nullptr;
  tab_subscriptions_.clear();
  Close(/*notify_delegate=*/true);
}

void FedCmAccountSelectionView::ModalUIChanged(tabs::TabInterface* tab) {
  UpdateDialogVisibilityAndPosition();
}

void FedCmAccountSelectionView::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // Whether we clear tab_ depends on whether the tab is going to be destroyed,
  // or re-inserted into another window.
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      tab_ = nullptr;
      tab_subscriptions_.clear();
      break;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      break;
  }
  // If the tab is going to be detached from the window then we must clear all
  // window-scoped UI.
  Close(/*notify_delegate=*/true);
}

FedCmModalDialogView* FedCmAccountSelectionView::GetPopupWindowForTesting() {
  return popup_window_.get();
}

void FedCmAccountSelectionView::OnPopupWindowDestroyed() {
  bool user_close_cancels_flow = popup_window_->UserCloseCancelsFlow();
  popup_window_.reset();
  if (!user_close_cancels_flow) {
    // TODO(https://crbug.com/377803489): Delete this code.
    UpdateDialogVisibilityAndPosition();
    return;
  }
  Close(/*notify_delegate=*/true);
}

bool FedCmAccountSelectionView::NotifyDelegateOfAccountSelection(
    const Account& account,
    const content::IdentityProviderData& idp_data) {
  DCHECK(state_ == State::VERIFYING || state_ == State::AUTO_REAUTHN);

  base::WeakPtr<FedCmAccountSelectionView> weak_ptr(
      weak_ptr_factory_.GetWeakPtr());
  delegate_->OnAccountSelected(
      idp_data.idp_metadata.config_url, account.id,
      account.login_state.value_or(Account::LoginState::kSignUp));

  // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
  // See https://crbug.com/1393650 for details.
  return static_cast<bool>(weak_ptr);
}

void FedCmAccountSelectionView::ShowVerifyingSheet(
    const IdentityRequestAccountPtr& account) {
  const std::u16string title =
      state_ == State::AUTO_REAUTHN
          ? l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE_AUTO_REAUTHN)
          : l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE);
  account_selection_view_->ShowVerifyingSheet(account, title);
}

SheetType FedCmAccountSelectionView::GetSheetType() {
  switch (state_) {
    case State::IDP_SIGNIN_STATUS_MISMATCH:
      return webid::SheetType::SIGN_IN_TO_IDP_STATIC;

    case State::SINGLE_ACCOUNT_PICKER:
    case State::MULTI_ACCOUNT_PICKER:
    case State::REQUEST_PERMISSION:
    case State::NEWLY_LOGGED_IN_ACCOUNT_PICKER:
      return webid::SheetType::ACCOUNT_SELECTION;

    case State::VERIFYING:
      return webid::SheetType::VERIFYING;

    case State::AUTO_REAUTHN:
      return webid::SheetType::AUTO_REAUTHN;

    case State::SIGN_IN_ERROR:
      return webid::SheetType::SIGN_IN_ERROR;

    case State::LOADING:
      return webid::SheetType::LOADING;
  }
}

void FedCmAccountSelectionView::Close(bool notify_delegate) {
  if (!GetDialogWidget()) {
    CHECK(!account_selection_view_);
    return;
  }

  // The widget is synchronously destroyed.
  CloseWidget(notify_delegate, views::Widget::ClosedReason::kUnspecified);
}

views::Widget* FedCmAccountSelectionView::GetDialogWidget() {
  return dialog_widget_.get();
}

std::unique_ptr<views::Widget> FedCmAccountSelectionView::CreateDialogWidget() {
  std::unique_ptr<views::Widget> dialog_widget;
  if (dialog_type_ == DialogType::BUBBLE) {
    auto* bubble =
        static_cast<AccountSelectionBubbleView*>(account_selection_view_);
    dialog_widget =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            bubble, views::Widget::InitParams::CLIENT_OWNS_WIDGET));
  } else {
    // Create and show the dialog widget. This is functionally a tab-modal
    // dialog.
    auto* modal =
        static_cast<AccountSelectionModalView*>(account_selection_view_);
    gfx::NativeWindow top_level_native_window =
        web_contents()->GetTopLevelNativeWindow();
    views::Widget* top_level_widget =
        views::Widget::GetWidgetForNativeWindow(top_level_native_window);
    dialog_widget = base::WrapUnique(views::DialogDelegate::CreateDialogWidget(
        modal, /*context=*/gfx::NativeWindow(),
        /*parent=*/top_level_widget->GetNativeView()));
  }

  extensions::SecurityDialogTracker::GetInstance()->AddSecurityDialog(
      dialog_widget.get());
  pip_occlusion_observation_ =
      std::make_unique<ScopedPictureInPictureOcclusionObservation>(this);
  pip_occlusion_observation_->Observe(dialog_widget.get());
  return dialog_widget;
}

std::unique_ptr<FedCmModalDialogView>
FedCmAccountSelectionView::CreatePopupWindow() {
  return std::make_unique<FedCmModalDialogView>(delegate_->GetWebContents(),
                                                this);
}

scoped_refptr<network::SharedURLLoaderFactory>
FedCmAccountSelectionView::GetURLLoaderFactory() {
  return SystemNetworkContextManager::GetInstance()
      ->GetSharedURLLoaderFactory();
}

views::View* FedCmAccountSelectionView::GetAnchorView() {
  return tab_->GetBrowserWindowInterface()->GetWebView();
}

AccountSelectionViewBase* FedCmAccountSelectionView::CreateDialogView(
    bool has_modal_support,
    const content::RelyingPartyData& rp_data,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DialogType* out_dialog_type) {
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      GetURLLoaderFactory();

  if (rp_mode == blink::mojom::RpMode::kActive && has_modal_support) {
    *out_dialog_type = DialogType::MODAL;
    return new AccountSelectionModalView(rp_data, idp_title, rp_context,
                                         GetURLLoaderFactory(), this);
  } else {
    *out_dialog_type = DialogType::BUBBLE;
    return new AccountSelectionBubbleView(rp_data, idp_title, rp_context,
                                          GetAnchorView(),
                                          GetURLLoaderFactory(), this);
  }
}

bool FedCmAccountSelectionView::IsIdpSigninPopupOpen() {
  // The IDP sign-in pop-up can be triggered either from the user triggering a
  // active flow with no accounts while the loading dialog is shown, the
  // "Continue" button on the mismatch dialog or the "Add Account" button from
  // an account picker.
  return popup_window_ && (state_ == State::LOADING ||
                           state_ == State::IDP_SIGNIN_STATUS_MISMATCH ||
                           state_ == State::SINGLE_ACCOUNT_PICKER ||
                           state_ == State::MULTI_ACCOUNT_PICKER ||
                           state_ == State::NEWLY_LOGGED_IN_ACCOUNT_PICKER);
}

void FedCmAccountSelectionView::PrimaryMainFrameWasResized(bool width_changed) {
  UpdateDialogVisibilityAndPosition();
}

void FedCmAccountSelectionView::HideDialogWidget() {
  GetDialogWidget()->Hide();
  scoped_ignore_input_events_.reset();
  tab_accept_mouse_events_.reset();
  // TODO(crbug.com/331166928): This is only null in one test. Fix the test to
  // match production.
  if (input_protector_) {
    input_protector_->VisibilityChanged(false);
  }
}

base::WeakPtr<FedCmAccountSelectionView>
FedCmAccountSelectionView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FedCmAccountSelectionView::TabForegrounded(tabs::TabInterface* tab) {
  UpdateDialogVisibilityAndPosition();
}

void FedCmAccountSelectionView::TabWillEnterBackground(
    tabs::TabInterface* tab) {
  // The reason this does not use UpdateDialogVisibilityAndPosition() is because
  // the tab has not yet entered the background, and so tab->IsInForeground()
  // returns true. If it's important to simplify this then we should add
  // TabInterface::RegisterDidEnterBackground().
  if (GetDialogWidget()) {
    HideDialogWidget();
  }
}

void FedCmAccountSelectionView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const gfx::Image& rp_icon,
    bool show_back_button) {
  state_ = State::MULTI_ACCOUNT_PICKER;
  account_selection_view_->ShowMultiAccountPicker(accounts, idp_list, rp_icon,
                                                  show_back_button);
}

void FedCmAccountSelectionView::OnOcclusionStateChanged(bool occluded) {
  if (GetDialogWidget()) {
    GetDialogWidget()->GetContentsView()->SetEnabled(!occluded);
  }
  // SetEnabled does not always seem sufficient for unknown reasons, so we
  // also set this boolean to ignore input. But we still call SetEnabled
  // to visually indicate that input is disabled where possible.
  is_occluded_by_pip_ = occluded;
}

void FedCmAccountSelectionView::LogDialogDismissal(
    DismissReason dismiss_reason) {
  // Check is_mismatch_continue_clicked_ to ensure we don't record this metric
  // after MismatchDialogResult::kContinued has been recorded.
  if (state_ == State::IDP_SIGNIN_STATUS_MISMATCH &&
      !is_mismatch_continue_clicked_) {
    UMA_HISTOGRAM_ENUMERATION(
        "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
        dismiss_reason == DismissReason::kCloseButton
            ? MismatchDialogResult::kDismissedByCloseIcon
            : MismatchDialogResult::kDismissedForOtherReasons);
  }

  // Pop-up window can only be opened through clicking the "Continue" button on
  // the mismatch dialog. Hence, we record the outcome only after the dialog is
  // closed.
  if (is_mismatch_continue_clicked_ && popup_window_state_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.IdpSigninStatus.PopupWindowResult",
                              *popup_window_state_);
  }

  ukm::SourceId source_id =
      (web_contents() && web_contents()->GetPrimaryMainFrame())
          ? web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()
          : ukm::kInvalidSourceId;

  // If a modal account chooser was open, record the outcome.
  if (modal_account_chooser_state_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Button.AccountChooserResult",
                              *modal_account_chooser_state_);
    if (source_id != ukm::kInvalidSourceId) {
      ukm::builders::Blink_FedCm(source_id)
          .SetButton_AccountChooserResult(
              static_cast<int>(*modal_account_chooser_state_))
          .Record(ukm::UkmRecorder::Get());
    }
  }

  // If a modal loading dialog was open, record the outcome.
  if (modal_loading_dialog_state_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Button.LoadingDialogResult",
                              *modal_loading_dialog_state_);
    if (source_id != ukm::kInvalidSourceId) {
      ukm::builders::Blink_FedCm(source_id)
          .SetButton_LoadingDialogResult(
              static_cast<int>(*modal_loading_dialog_state_))
          .Record(ukm::UkmRecorder::Get());
    }
  }

  // If a modal disclosure dialog was open, record the outcome.
  if (modal_disclosure_dialog_state_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Button.DisclosureDialogResult",
                              *modal_disclosure_dialog_state_);
    if (source_id != ukm::kInvalidSourceId) {
      ukm::builders::Blink_FedCm(source_id)
          .SetButton_DisclosureDialogResult(
              static_cast<int>(*modal_disclosure_dialog_state_))
          .Record(ukm::UkmRecorder::Get());
    }
  }
}

void FedCmAccountSelectionView::CloseWidget(
    bool notify_delegate,
    views::Widget::ClosedReason reason) {
  DismissReason dismiss_reason =
      reason == views::Widget::ClosedReason::kCloseButtonClicked
          ? DismissReason::kCloseButton
          : DismissReason::kOther;
  LogDialogDismissal(dismiss_reason);
  input_protector_.reset();

  pip_occlusion_observation_.reset();

  // Implicitly owned by the dialog widget. Must clear to avoid UaF.
  account_selection_view_ = nullptr;
  scoped_ignore_input_events_.reset();
  dialog_widget_.reset();

  // This delegate call can result in synchronous destruction of `this`. Avoid
  // referencing any members after this call.
  if (notify_delegate) {
    delegate_->OnDismiss(dismiss_reason);
  }
}

void FedCmAccountSelectionView::OnUserClosedDialog(
    views::Widget::ClosedReason reason) {
  // The user closing the dialog will not cancel out of the fedCM flow if the
  // dialog is in the AUTO_REAUTHN or VERIFYING states, since at that point the
  // dialog is just informative.
  bool notify_delegate =
      state_ != State::AUTO_REAUTHN && state_ != State::VERIFYING;
  CloseWidget(notify_delegate, reason);
}

void FedCmAccountSelectionView::UpdateDialogVisibilityAndPosition() {
  if (!dialog_widget_) {
    return;
  }

  bool should_show_dialog = tab_->IsActivated();

  if (dialog_type_ == DialogType::BUBBLE) {
    // Hide the bubble dialog if it can't fit.
    if (!CanFitInWebContents()) {
      should_show_dialog = false;
    }

    // Or if a popup is showing.
    if (popup_window_) {
      should_show_dialog = false;
    }

    // Or if we want to hide until Show*() is called.
    if (hide_dialog_widget_after_idp_login_popup_) {
      should_show_dialog = false;
    }

    // Or if a tab modal UI is showing (which means we can't show a new modal).
    if (!tab_->CanShowModalUI()) {
      should_show_dialog = false;
    }
  }

  if (should_show_dialog) {
    UpdateDialogPosition();
    if (!dialog_widget_->IsVisible()) {
      ShowDialogWidget();
    }
    return;
  }

  HideDialogWidget();
}

void FedCmAccountSelectionView::ResetDialogWidgetStateOnAnyShow() {
  accounts_widget_shown_callback_.Reset();
  hide_dialog_widget_after_idp_login_popup_ = false;
}

}  // namespace webid
