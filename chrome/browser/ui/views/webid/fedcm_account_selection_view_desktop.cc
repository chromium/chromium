// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

using DismissReason = content::IdentityRequestDialogController::DismissReason;
using SheetType = AccountSelectionView::SheetType;

// static
int AccountSelectionView::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  // TODO(crbug.com/348673144): Decide whether to keep circle cropping IDP
  // icons.
  return (rp_mode == blink::mojom::RpMode::kActive ? kModalIdpIconSize
                                                   : kBubbleIdpIconSize) /
         FedCmAccountSelectionView::kMaskableWebIconSafeZoneRatio;
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

FedCmAccountSelectionView::FedCmAccountSelectionView(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate),
      content::WebContentsObserver(delegate->GetWebContents()),
      is_web_contents_visible_(delegate->GetWebContents()->GetVisibility() ==
                               content::Visibility::VISIBLE) {
}

FedCmAccountSelectionView::~FedCmAccountSelectionView() {
  notify_delegate_of_dismiss_ = false;
  is_modal_closed_but_accounts_fetch_pending_ = false;
  Close();

  // We use this boolean to record metrics in Close(), reset it after Close().
  is_mismatch_continue_clicked_ = false;
}

void FedCmAccountSelectionView::ShowDialogWidget() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser &&
      browser->tab_strip_model()->GetActiveWebContents() != web_contents()) {
    // This is unexpected since we should never reach this codepath when the
    // WebContents is not the active one. Dump to get debug info on when this
    // happens, and do not show the widget in this case.
    base::debug::DumpWithoutCrashing();
    return;
  }
  input_protector_->VisibilityChanged(true);
  // An active widget would steal the focus when displayed, this would lead
  // to some unexpected consequences. e.g.
  //   1. links/buttons from the web contents area would require two clicks,
  //   one to focus on the content area and one to focus on the clickable
  //   2. user typing will be interrupted because the widget that's not
  //   gated by user gesture would take the focus
  // TODO(crbug.com/41482141): figure out how to address this issue without
  // causing additional problems such as obscuring other browser UIs.
  GetDialogWidget()->Show();
}

bool FedCmAccountSelectionView::Show(
    const std::string& rp_for_display,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::vector<IdentityRequestAccountPtr>& new_accounts) {
  // If IDP sign-in pop-up is open, we delay the showing of the accounts dialog
  // until the pop-up is destroyed.
  if (IsIdpSigninPopupOpen()) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupNotClosedByIdp;
    // We need to use ShowNoReturn() here because it is not allowed to bind
    // WeakPtrs to methods with return values.
    show_accounts_dialog_callback_ =
        base::BindOnce(base::IgnoreResult(&FedCmAccountSelectionView::Show),
                       weak_ptr_factory_.GetWeakPtr(), rp_for_display, idp_list,
                       accounts, sign_in_mode, rp_mode, new_accounts);
    // This is considered successful since we are intentionally delaying showing
    // the UI.
    return true;
  }

  accounts_displayed_callback_ =
      base::BindOnce(&FedCmAccountSelectionView::OnAccountsDisplayed,
                     weak_ptr_factory_.GetWeakPtr());

  // TODO(crbug.com/41491333): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = true;

  idp_list_ = idp_list;
  accounts_ = accounts;
  new_accounts_ = new_accounts;
  started_as_single_returning_account_ = false;
  last_multi_account_is_choose_an_account_ = false;

  size_t accounts_or_mismatches_size = accounts.size();
  bool supports_add_account = false;
  blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn;
  for (const auto& identity_provider : idp_list) {
    supports_add_account |=
        identity_provider->idp_metadata.supports_add_account;
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

  size_t returning_accounts_size =
      std::count_if(accounts.begin(), accounts.end(), [](const auto& account) {
        return account->login_state ==
               content::IdentityRequestAccount::LoginState::kSignIn;
      });

  std::optional<std::u16string> idp_title =
      idp_list_.size() == 1u
          ? std::make_optional<std::u16string>(
                base::UTF8ToUTF16(idp_list_[0]->idp_for_display))
          : std::nullopt;
  rp_for_display_ = base::UTF8ToUTF16(rp_for_display);

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead. We also reset for widget multi IDP to recalculate the title
  // and other parts of the header.
  if ((rp_mode == blink::mojom::RpMode::kPassive && idp_list_.size() > 1) ||
      (rp_mode == blink::mojom::RpMode::kActive && !has_modal_support)) {
    MaybeResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        rp_for_display_, idp_title, rp_context, rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return false;
    }
  }

  if (sign_in_mode == Account::SignInMode::kAuto) {
    // In Active mode the first UI we should should be a loading modal. When
    // auto re-authn is triggered, we transform the UI to the single account
    // that's available to notify user that they are being signed in with that
    // account.
    if (GetDialogType() == DialogType::MODAL) {
      state_ = State::SINGLE_ACCOUNT_PICKER;
      account_selection_view_->ShowSingleAccountConfirmDialog(
          *accounts[0],
          /*show_back_button=*/false);
    }

    state_ = State::AUTO_REAUTHN;

    // When auto re-authn flow is triggered, the parameter
    // `idp_list_` would only include the single returning
    // account and its IDP.
    DCHECK_EQ(idp_list_.size(), 1u);
    DCHECK_EQ(accounts.size(), 1u);
    // If ShowVerifyingSheet returns false, `this` got deleted, so just
    // return.
    if (!ShowVerifyingSheet(*accounts[0], *idp_list_[0])) {
      return false;
    }
  } else if (!new_accounts.empty()) {
    // When we just logged in to an account that is not a single returning
    // account: on the modal, we'd show all the accounts and on the bubble, we'd
    // show only the new accounts.
    const content::IdentityProviderData& new_idp_data =
        *new_accounts_[0]->identity_provider;

    if (GetDialogType() == DialogType::MODAL) {
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
        // ShowVerifyingSheet will call delegate_->OnAccountSelected to proceed.
        if (!ShowVerifyingSheet(*new_accounts_[0], new_idp_data)) {
          return false;
        }
      } else if (should_show_request_permission_dialog) {
        state_ = State::REQUEST_PERMISSION;
        account_selection_view_->ShowRequestPermissionDialog(*new_accounts_[0],
                                                             new_idp_data);
      } else {
        // Normally we'd show the request permission dialog but without the
        // disclosure text, there is no material difference between the account
        // picker and the request permission dialog. We show the account picker
        // with most recently signed in accounts at the top to reduce the
        // exposure of extra UI surfaces and to work around the account picker
        // not having a back button.
        ShowMultiAccountPicker(accounts_, idp_list_,
                               /*show_back_button=*/false,
                               /*is_choose_an_account=*/false);
      }
    } else {
      if (new_accounts_.size() == 1u) {
        state_ = State::SINGLE_ACCOUNT_PICKER;
        account_selection_view_->ShowSingleAccountConfirmDialog(
            *new_accounts_[0],
            /*show_back_button=*/accounts_or_mismatches_size > 1u ||
                supports_add_account);
      } else {
        ShowMultiAccountPicker(
            new_accounts_, {new_accounts_[0]->identity_provider},
            /*show_back_button=*/accounts_or_mismatches_size >
                new_accounts_.size(),
            /*is_choose_an_account=*/false);
        // Override the state to NEWLY_LOGGED_IN_ACCOUNT_PICKER so the back
        // button works correctly.
        state_ = State::NEWLY_LOGGED_IN_ACCOUNT_PICKER;
      }
    }
  } else if (idp_list_.size() == 1u && accounts_or_mismatches_size == 1u) {
    if (GetDialogType() == DialogType::MODAL) {
      state_ = State::SINGLE_ACCOUNT_PICKER;
      account_selection_view_->ShowSingleAccountConfirmDialog(
          *accounts_[0],
          /*show_back_button=*/false);
    } else if (supports_add_account) {
      // The logic to support add account is in ShowMultiAccountPicker for the
      // bubble dialog.
      ShowMultiAccountPicker(accounts_, idp_list_, /*show_back_button=*/false,
                             /*is_choose_an_account=*/false);
    } else {
      state_ = State::SINGLE_ACCOUNT_PICKER;
      account_selection_view_->ShowSingleAccountConfirmDialog(
          *accounts_[0],
          /*show_back_button=*/false);
    }
  } else if (idp_list_.size() > 1u && returning_accounts_size == 1u) {
    // For now we only highlight the single returning account in the multi IDP
    // case, but in the future we may want to do so in the single IDP case as
    // well.
    state_ = State::SINGLE_RETURNING_ACCOUNT_PICKER;
    started_as_single_returning_account_ = true;
    account_selection_view_->ShowSingleReturningAccountDialog(accounts_,
                                                              idp_list_);
  } else {
    ShowMultiAccountPicker(accounts_, idp_list_,
                           /*show_back_button=*/false,
                           /*is_choose_an_account=*/false);
  }

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  // The popup_window_state_ check is for the case when we received new accounts
  // while the modal dialog is visible and we are called from CloseModalDialog.
  // Because the modal dialog is now closed, we should show the account chooser
  // now.
  if (create_view || is_modal_closed_but_accounts_fetch_pending_ ||
      (popup_window_state_ &&
       *popup_window_state_ ==
           PopupWindowResult::kAccountsReceivedAndPopupNotClosedByIdp)) {
    is_modal_closed_but_accounts_fetch_pending_ = false;
    if (is_web_contents_visible_ &&
        account_selection_view_->CanFitInWebContents()) {
      ShowDialogWidget();
      if (accounts_displayed_callback_) {
        std::move(accounts_displayed_callback_).Run();
      }
    }
  }
  // Else:
  // Do not force show the dialog. The dialog may be purposefully hidden if the
  // WebContents are hidden.

  if (!idp_close_popup_time_.is_null()) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupClosedByIdp;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Blink.FedCm.IdpSigninStatus."
        "IdpClosePopupToBrowserShowAccountsDuration",
        base::TimeTicks::Now() - idp_close_popup_time_);
  }

  if (GetDialogType() == DialogType::MODAL &&
      (state_ == State::SINGLE_ACCOUNT_PICKER ||
       state_ == State::MULTI_ACCOUNT_PICKER)) {
    // This is a placeholder assuming the tab containing the account chooser
    // will be closed. This will be updated upon user action i.e. clicking on
    // account row, cancel button or use other account button. If we do not
    // receive any of these actions by time the dialog is closed, it means our
    // placeholder assumption is true i.e. the user has closed the tab.
    modal_account_chooser_state_ = AccountChooserResult::kTabClosed;
  }
  return true;
}

void FedCmAccountSelectionView::OnAccountsDisplayed() {
  delegate_->OnAccountsDisplayed();
}

bool FedCmAccountSelectionView::ShowFailureDialog(
    const std::string& rp_for_display,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata) {
  state_ = State::IDP_SIGNIN_STATUS_MISMATCH;

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
    MaybeResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  rp_for_display_ = base::UTF8ToUTF16(rp_for_display);
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        rp_for_display_, base::UTF8ToUTF16(idp_etld_plus_one), rp_context,
        rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return false;
    }
  }

  account_selection_view_->ShowFailureDialog(
      base::UTF8ToUTF16(idp_etld_plus_one), idp_metadata);

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (create_view || is_modal_closed_but_accounts_fetch_pending_) {
    is_modal_closed_but_accounts_fetch_pending_ = false;
    if (is_web_contents_visible_ &&
        account_selection_view_->CanFitInWebContents()) {
      ShowDialogWidget();
    }
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
  return true;
}

bool FedCmAccountSelectionView::ShowErrorDialog(
    const std::string& rp_for_display,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  state_ = State::SIGN_IN_ERROR;
  notify_delegate_of_dismiss_ = true;

  // TODO(crbug.com/41491333): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = false;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead. We also reset for widget multi IDP to recalculate the title
  // and other parts of the header.
  if ((rp_mode == blink::mojom::RpMode::kPassive && idp_list_.size() > 1) ||
      (rp_mode == blink::mojom::RpMode::kActive && !has_modal_support)) {
    MaybeResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        rp_for_display_, base::UTF8ToUTF16(idp_etld_plus_one), rp_context,
        rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return false;
    }
  }

  account_selection_view_->ShowErrorDialog(
      base::UTF8ToUTF16(idp_etld_plus_one), idp_metadata, error);

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (is_web_contents_visible_ &&
      account_selection_view_->CanFitInWebContents()) {
    ShowDialogWidget();
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
  return true;
}

bool FedCmAccountSelectionView::ShowLoadingDialog(
    const std::string& rp_for_display,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode) {
  CHECK(rp_mode == blink::mojom::RpMode::kActive);

  state_ = State::LOADING;
  notify_delegate_of_dismiss_ = true;

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        base::UTF8ToUTF16(rp_for_display), base::UTF8ToUTF16(idp_etld_plus_one),
        rp_context, rp_mode,
        /*has_modal_support=*/true);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return false;
    }
  }

  account_selection_view_->ShowLoadingDialog();

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return false;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (create_view && is_web_contents_visible_) {
    ShowDialogWidget();
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
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
  return std::nullopt;
}

void FedCmAccountSelectionView::OnTabForegrounded() {
  is_web_contents_visible_ = true;
  if (!IsDialogWidgetReady()) {
    return;
  }
  if (ShouldShowDialogWidget()) {
    UpdateAndShowDialogWidget();
  }
}

void FedCmAccountSelectionView::OnTabBackgrounded() {
  is_web_contents_visible_ = false;
  if (GetDialogWidget()) {
    HideDialogWidget();
  }
}

void FedCmAccountSelectionView::PrimaryPageChanged(content::Page& page) {
  // Close the dialog when the user navigates within the same tab.
  Close();
}

void FedCmAccountSelectionView::SetInputEventActivationProtectorForTesting(
    std::unique_ptr<views::InputEventActivationProtector> input_protector) {
  input_protector_ = std::move(input_protector);
}

void FedCmAccountSelectionView::SetIdpSigninPopupWindowForTesting(
    std::unique_ptr<FedCmModalDialogView> idp_signin_popup_window) {
  popup_window_ = std::move(idp_signin_popup_window);
}

AccountSelectionViewBase* FedCmAccountSelectionView::CreateAccountSelectionView(
    const std::u16string& rp_for_display,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    bool has_modal_support) {
  content::WebContents* web_contents = delegate_->GetWebContents();
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // Reject the API if the browser is not found.
  // TODO(crbug.com/342216390): It is unclear why there are callers attempting
  // FedCM when some of these checks fail.
  if (!browser) {
    return nullptr;
  }

  if (rp_mode == blink::mojom::RpMode::kActive && has_modal_support) {
    dialog_type_ = DialogType::MODAL;
    return new AccountSelectionModalView(
        rp_for_display, idp_title, rp_context, web_contents,
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory(),
        this, this);
  }

  dialog_type_ = DialogType::BUBBLE;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view = browser_view->contents_web_view();

  return new AccountSelectionBubbleView(
      rp_for_display, idp_title, rp_context, web_contents, anchor_view,
      SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory(),
      this, this);
}

void FedCmAccountSelectionView::OnWidgetDestroying(views::Widget* widget) {
  DismissReason dismiss_reason =
      (GetDialogWidget()->closed_reason() ==
       views::Widget::ClosedReason::kCloseButtonClicked)
          ? DismissReason::kCloseButton
          : DismissReason::kOther;
  OnDismiss(dismiss_reason);
}

void FedCmAccountSelectionView::OnAccountSelected(
    const Account& account,
    const content::IdentityProviderData& idp_data,
    const ui::Event& event) {
  DCHECK(state_ != State::IDP_SIGNIN_STATUS_MISMATCH);
  DCHECK(state_ != State::AUTO_REAUTHN);

  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      account_selection_view_->IsOccluded()) {
    return;
  }

  if (modal_account_chooser_state_) {
    modal_account_chooser_state_ = AccountChooserResult::kAccountRow;
  }

  // If the account is a returning user or if the account is selected from UI
  // which shows the disclosure text or if the dialog doesn't need to ask for
  // the user's permission to share their id/email/name/picture, show the
  // verifying sheet.
  if (account.login_state != Account::LoginState::kSignUp ||
      state_ == State::REQUEST_PERMISSION ||
      (state_ == State::SINGLE_ACCOUNT_PICKER &&
       GetDialogType() == DialogType::BUBBLE) ||
      idp_data.disclosure_fields.empty()) {
    state_ = State::VERIFYING;
    ShowVerifyingSheet(account, idp_data);
    return;
  }

  // At this point, the account is a non-returning user. If the dialog is modal,
  // we'd request permission through the request permission dialog.
  if (GetDialogType() == DialogType::MODAL) {
    state_ = State::REQUEST_PERMISSION;
    account_selection_view_->ShowRequestPermissionDialog(account, idp_data);
    return;
  }

  // At this point, the account is a non-returning user, the dialog is a bubble
  // and it is a multi account picker, there is no disclosure text on the dialog
  // so we'd request permission through a single account dialog.
  state_ = State::SINGLE_ACCOUNT_PICKER;
  account_selection_view_->ShowSingleAccountConfirmDialog(
      account, /*show_back_button=*/true);
}

void FedCmAccountSelectionView::OnLinkClicked(LinkType link_type,
                                              const GURL& url,
                                              const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      account_selection_view_->IsOccluded()) {
    return;
  }
  ShowUrl(link_type, url);
}

void FedCmAccountSelectionView::OnBackButtonClicked() {
  // No need to protect input here since back cannot be the first event.

  // If the dialog type is modal and there is only one IDP and one account, show
  // the single account picker.
  if (GetDialogType() == DialogType::MODAL && idp_list_.size() == 1u &&
      accounts_.size() == 1u) {
    state_ = State::SINGLE_ACCOUNT_PICKER;
    account_selection_view_->ShowSingleAccountConfirmDialog(
        *accounts_[0], /*show_back_button=*/false);
    return;
  }
  // If the back button was clicked while on the multi account picker, go back
  // to the single returning account.
  if (state_ == State::MULTI_ACCOUNT_PICKER) {
    state_ = State::SINGLE_RETURNING_ACCOUNT_PICKER;
    account_selection_view_->ShowSingleReturningAccountDialog(accounts_,
                                                              idp_list_);
    return;
  }
  ShowMultiAccountPicker(
      accounts_, idp_list_,
      /*show_back_button=*/started_as_single_returning_account_,
      /*is_choose_an_account=*/last_multi_account_is_choose_an_account_);
}

void FedCmAccountSelectionView::OnCloseButtonClicked(const ui::Event& event) {
  // Because the close button is a safe button to click and may be visible
  // even when the widget is (partially) occluded, we do not check
  // IsOccluded here.
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  if (GetDialogType() == DialogType::BUBBLE) {
    UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.CloseVerifySheet.Desktop",
                          state_ == State::VERIFYING);

    // Record the sheet type that the user was closing.
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.ClosedSheetType.Desktop",
                              GetSheetType(), SheetType::COUNT);
  }

  // Check that state_ at the time of closing is an account chooser, otherwise,
  // closing other dialogs can override the modal_account_chooser_state_.
  if (modal_account_chooser_state_ && (state_ == State::SINGLE_ACCOUNT_PICKER ||
                                       state_ == State::MULTI_ACCOUNT_PICKER)) {
    modal_account_chooser_state_ = AccountChooserResult::kCancelButton;
  }

  // This may have been set to false when the user triggers the use other
  // account pop-up on the modal to prevent dismissing when the user closes the
  // pop-up. However if the user clicks cancel, we should dismiss so we should
  // set this back to true.
  notify_delegate_of_dismiss_ = true;
  GetDialogWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void FedCmAccountSelectionView::OnLoginToIdP(const GURL& idp_config_url,
                                             const GURL& idp_login_url,
                                             const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      account_selection_view_->IsOccluded()) {
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
    modal_account_chooser_state_ = AccountChooserResult::kUseOtherAccountButton;
  }
}

void FedCmAccountSelectionView::OnGotIt(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      account_selection_view_->IsOccluded()) {
    return;
  }

  delegate_->OnDismiss(DismissReason::kGotItButton);
}

void FedCmAccountSelectionView::OnMoreDetails(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event) ||
      account_selection_view_->IsOccluded()) {
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
    popup_window_ = std::make_unique<FedCmModalDialogView>(
        delegate_->GetWebContents(), this);
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
  if (GetDialogType() != DialogType::MODAL) {
    if (GetDialogWidget()) {
      HideDialogWidget();
    }
  }

  // The modal should not be dismissed if it a use other account pop-up, which
  // can only be triggered from an account selection sheet.
  if (GetDialogType() == DialogType::MODAL &&
      GetSheetType() == SheetType::ACCOUNT_SELECTION) {
    notify_delegate_of_dismiss_ = false;
    return popup_window_->ShowPopupWindow(url);
  }

  // If this happens after ShowVerifyingSheet, notify_delegate_of_dismiss_ may
  // be false. However, if the user closes the popup, we do want to call
  // OnDismiss to ensure the request is cancelled, so set it to true here.
  notify_delegate_of_dismiss_ = true;
  return popup_window_->ShowPopupWindow(url);
}

void FedCmAccountSelectionView::CloseModalDialog() {
  auto show_accounts_callback = std::move(show_accounts_dialog_callback_);
  if (popup_window_) {
    // Programmatic closure should never notify the delegate. If necessary the
    // caller will take care of that, e.g. by aborting the flow.
    notify_delegate_of_dismiss_ = false;
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
      is_modal_closed_but_accounts_fetch_pending_ = true;
      idp_close_popup_time_ = base::TimeTicks::Now();
      popup_window_state_ =
          PopupWindowResult::kAccountsNotReceivedAndPopupClosedByIdp;
    }

    auto popup_window = std::move(popup_window_);
    popup_window->ClosePopupWindow();
    // We suspect that ClosePopupWindow may delete `this` under unknown
    // circumstances. Do not access member variables after this point.
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

void FedCmAccountSelectionView::OnChooseAnAccountClicked() {
  ShowMultiAccountPicker(accounts_, idp_list_,
                         /*show_back_button=*/true,
                         /*is_choose_an_account=*/true);
  base::UmaHistogramBoolean("Blink.FedCm.ChooseAnAccountSelected.Desktop",
                            true);
}

void FedCmAccountSelectionView::OnPopupWindowDestroyed() {
  popup_window_.reset();

  if (!notify_delegate_of_dismiss_) {
    return;
  }

  // This triggers the OnDismiss call to notify delegate_
  Close();
}

bool FedCmAccountSelectionView::ShowVerifyingSheet(
    const Account& account,
    const content::IdentityProviderData& idp_data) {
  DCHECK(state_ == State::VERIFYING || state_ == State::AUTO_REAUTHN);
  notify_delegate_of_dismiss_ = false;

  base::WeakPtr<FedCmAccountSelectionView> weak_ptr(
      weak_ptr_factory_.GetWeakPtr());
  delegate_->OnAccountSelected(idp_data.idp_metadata.config_url, account);
  // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
  // See https://crbug.com/1393650 for details.
  if (!weak_ptr) {
    return false;
  }

  const std::u16string title =
      state_ == State::AUTO_REAUTHN
          ? l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE_AUTO_REAUTHN)
          : l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE);
  account_selection_view_->ShowVerifyingSheet(account, title);
  return true;
}

SheetType FedCmAccountSelectionView::GetSheetType() {
  switch (state_) {
    case State::IDP_SIGNIN_STATUS_MISMATCH:
      return SheetType::SIGN_IN_TO_IDP_STATIC;

    case State::SINGLE_ACCOUNT_PICKER:
    case State::MULTI_ACCOUNT_PICKER:
    case State::REQUEST_PERMISSION:
    case State::SINGLE_RETURNING_ACCOUNT_PICKER:
    case State::NEWLY_LOGGED_IN_ACCOUNT_PICKER:
      return SheetType::ACCOUNT_SELECTION;

    case State::VERIFYING:
      return SheetType::VERIFYING;

    case State::AUTO_REAUTHN:
      return SheetType::AUTO_REAUTHN;

    case State::SIGN_IN_ERROR:
      return SheetType::SIGN_IN_ERROR;

    case State::LOADING:
      return SheetType::LOADING;
  }
}

void FedCmAccountSelectionView::Close() {
  if (!GetDialogWidget()) {
    // Normally this object is owned by the dialog widget, but here there
    // is no widget. We need to store the pointer before calling OnDismiss,
    // because OnDismiss might destroy this object.
    auto* view = account_selection_view_.get();
    account_selection_view_ = nullptr;
    delete view;

    if (delegate_ && notify_delegate_of_dismiss_) {
      delegate_->OnDismiss(DismissReason::kOther);
    }
    return;
  }

  GetDialogWidget()->Close();
  OnDismiss(DismissReason::kOther);
}

void FedCmAccountSelectionView::OnDismiss(DismissReason dismiss_reason) {
  if (!GetDialogWidget()) {
    return;
  }

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

  // If a modal account chooser was open, record the outcome.
  if (modal_account_chooser_state_) {
    UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.Button.AccountChooserResult",
                              *modal_account_chooser_state_);
  }

  MaybeResetAccountSelectionView();
  input_protector_.reset();

  if (notify_delegate_of_dismiss_) {
    delegate_->OnDismiss(dismiss_reason);
  }
}

base::WeakPtr<views::Widget> FedCmAccountSelectionView::GetDialogWidget() {
  return account_selection_view_ ? account_selection_view_->GetDialogWidget()
                                 : nullptr;
}

FedCmAccountSelectionView::DialogType
FedCmAccountSelectionView::GetDialogType() {
  return dialog_type_;
}

void FedCmAccountSelectionView::MaybeResetAccountSelectionView() {
  if (!account_selection_view_) {
    return;
  }
  account_selection_view_->CloseDialog();
  account_selection_view_ = nullptr;
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
  if (!GetDialogWidget() || GetDialogType() == DialogType::MODAL) {
    return;
  }

  if (account_selection_view_->CanFitInWebContents()) {
    if (!GetDialogWidget()->IsVisible() && is_web_contents_visible_) {
      account_selection_view_->UpdateDialogPosition();
      ShowDialogWidget();
    }
    return;
  }

  if (GetDialogWidget()->IsVisible()) {
    HideDialogWidget();
  }
}

bool FedCmAccountSelectionView::IsDialogWidgetReady() {
  return GetDialogWidget() && !popup_window_ &&
         !is_modal_closed_but_accounts_fetch_pending_;
}

bool FedCmAccountSelectionView::ShouldShowDialogWidget() {
  // TODO(crbug.com/340368623): Figure out what to do when active flow modal
  // cannot fit in web contents.
  return is_web_contents_visible_ &&
         (account_selection_view_->CanFitInWebContents() ||
          GetDialogType() == DialogType::MODAL);
}

void FedCmAccountSelectionView::UpdateAndShowDialogWidget() {
  // We need to update the dialog's position in case the window was resized
  // while it was not visible. The dialog position is already being updated
  // automatically if the window was resized while it is visible.
  account_selection_view_->UpdateDialogPosition();
  ShowDialogWidget();
  if (accounts_displayed_callback_) {
    std::move(accounts_displayed_callback_).Run();
  }
  GetDialogWidget()->widget_delegate()->SetCanActivate(true);
}

void FedCmAccountSelectionView::HideDialogWidget() {
  // On Mac, NativeWidgetMac::Activate() ignores the views::Widget visibility.
  // Make the views::Widget non-activatable while it is hidden to prevent the
  // views::Widget from being shown during focus traversal.
  // TODO(crbug.com/40239995): fix the issue on Mac.
  GetDialogWidget()->Hide();
  GetDialogWidget()->widget_delegate()->SetCanActivate(false);
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

void FedCmAccountSelectionView::ShowMultiAccountPicker(
    const std::vector<IdentityRequestAccountPtr>& accounts,
    const std::vector<IdentityProviderDataPtr>& idp_list,
    bool show_back_button,
    bool is_choose_an_account) {
  state_ = State::MULTI_ACCOUNT_PICKER;
  last_multi_account_is_choose_an_account_ = is_choose_an_account;
  account_selection_view_->ShowMultiAccountPicker(
      accounts, idp_list, show_back_button, is_choose_an_account);
}
