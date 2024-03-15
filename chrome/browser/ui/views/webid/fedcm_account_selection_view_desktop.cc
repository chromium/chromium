// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
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
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

using DismissReason = content::IdentityRequestDialogController::DismissReason;

// static
std::unique_ptr<AccountSelectionView> AccountSelectionView::Create(
    AccountSelectionView::Delegate* delegate) {
  return std::make_unique<FedCmAccountSelectionView>(delegate);
}

// static
int AccountSelectionView::GetBrandIconMinimumSize() {
  return 20 / FedCmAccountSelectionView::kMaskableWebIconSafeZoneRatio;
}

// static
int AccountSelectionView::GetBrandIconIdealSize() {
  // As only a single brand icon is selected and the user can have monitors with
  // different screen densities, make the ideal size be the size which works
  // with a high density display (if the OS supports high density displays).
  const float max_supported_scale =
      ui::GetScaleForMaxSupportedResourceScaleFactor();
  return round(GetBrandIconMinimumSize() * max_supported_scale);
}

FedCmAccountSelectionView::FedCmAccountSelectionView(
    AccountSelectionView::Delegate* delegate)
    : AccountSelectionView(delegate),
      content::WebContentsObserver(delegate->GetWebContents()),
      is_web_contents_visible_(delegate->GetWebContents()->GetVisibility() ==
                               content::Visibility::VISIBLE) {}

FedCmAccountSelectionView::~FedCmAccountSelectionView() {
  notify_delegate_of_dismiss_ = false;
  is_modal_closed_but_accounts_fetch_pending_ = false;
  should_destroy_dialog_widget_ = false;
  Close();

  // We use this boolean to record metrics in Close(), reset it after Close().
  is_mismatch_continue_clicked_ = false;
  TabStripModelObserver::StopObservingAll(this);
}

void FedCmAccountSelectionView::Show(
    const std::string& top_frame_etld_plus_one,
    const std::optional<std::string>& iframe_etld_plus_one,
    const std::vector<content::IdentityProviderData>&
        identity_provider_data_list,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    const std::optional<content::IdentityProviderData>& new_account_idp) {
  // If IDP sign-in modal dialog is open, we delay the showing of the accounts
  // dialog until the modal dialog is destroyed.
  // The sign-in modal dialog can be triggered either from the "Continue" button
  // on the mismatch dialog or the "Add Account" button from the account
  // chooser.
  if (popup_window_ && (state_ == State::IDP_SIGNIN_STATUS_MISMATCH ||
                        state_ == State::MULTI_ACCOUNT_PICKER)) {
    popup_window_state_ =
        PopupWindowResult::kAccountsReceivedAndPopupNotClosedByIdp;
    show_accounts_dialog_callback_ = base::BindOnce(
        &FedCmAccountSelectionView::Show, weak_ptr_factory_.GetWeakPtr(),
        top_frame_etld_plus_one, iframe_etld_plus_one,
        identity_provider_data_list, sign_in_mode, rp_mode, new_account_idp);
    return;
  }

  accounts_displayed_callback_ =
      base::BindOnce(&FedCmAccountSelectionView::OnAccountsDisplayed,
                     weak_ptr_factory_.GetWeakPtr());

  // TODO(crbug.com/1518356): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = sign_in_mode != Account::SignInMode::kAuto;

  idp_display_data_list_.clear();

  size_t accounts_size = 0u;
  blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn;
  for (const auto& identity_provider : identity_provider_data_list) {
    idp_display_data_list_.emplace_back(
        base::UTF8ToUTF16(identity_provider.idp_for_display),
        identity_provider.idp_metadata, identity_provider.client_metadata,
        identity_provider.accounts, identity_provider.request_permission,
        identity_provider.has_login_status_mismatch);
    // TODO(crbug.com/1406014): Decide what we should display if the IdPs use
    // different contexts here.
    rp_context = identity_provider.rp_context;
    accounts_size += identity_provider.accounts.size();
  }

  std::optional<std::u16string> idp_title =
      idp_display_data_list_.size() == 1u
          ? std::make_optional<std::u16string>(
                idp_display_data_list_[0].idp_etld_plus_one)
          : std::nullopt;
  top_frame_for_display_ = base::UTF8ToUTF16(top_frame_etld_plus_one);
  iframe_for_display_ = iframe_etld_plus_one
                            ? std::make_optional<std::u16string>(
                                  base::UTF8ToUTF16(*iframe_etld_plus_one))
                            : std::nullopt;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead.
  if (account_selection_view_ && rp_mode == blink::mojom::RpMode::kButton &&
      !has_modal_support) {
    ResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        top_frame_for_display_, iframe_for_display_, idp_title, rp_context,
        rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return;
    }
  }

  if (sign_in_mode == Account::SignInMode::kAuto) {
    state_ = State::AUTO_REAUTHN;

    // When auto re-authn flow is triggered, the parameter
    // `idp_display_data_list_` would only include the single returning
    // account and its IDP.
    DCHECK_EQ(idp_display_data_list_.size(), 1u);
    DCHECK_EQ(idp_display_data_list_[0].accounts.size(), 1u);
    // If ShowVerifyingSheet returns false, `this` got deleted, so just
    // return.
    if (!ShowVerifyingSheet(idp_display_data_list_[0].accounts[0],
                            idp_display_data_list_[0])) {
      return;
    }
  } else if (new_account_idp) {
    // When we just logged in to an account, show that account right away.
    state_ = State::REQUEST_PERMISSION;
    new_account_idp_display_data_ = IdentityProviderDisplayData(
        base::UTF8ToUTF16(new_account_idp->idp_for_display),
        new_account_idp->idp_metadata, new_account_idp->client_metadata,
        new_account_idp->accounts, new_account_idp->request_permission,
        new_account_idp->has_login_status_mismatch);

    if (GetDialogType() == DialogType::MODAL) {
      account_selection_view_->ShowRequestPermissionDialog(
          top_frame_for_display_, new_account_idp_display_data_->accounts[0],
          *new_account_idp_display_data_);
    } else {
      account_selection_view_->ShowSingleAccountConfirmDialog(
          top_frame_for_display_, iframe_for_display_,
          new_account_idp_display_data_->accounts[0],
          *new_account_idp_display_data_,
          /*show_back_button=*/accounts_size > 1u ? true : false);
    }
  } else if (idp_display_data_list_.size() == 1u && accounts_size == 1u &&
             !idp_display_data_list_[0].idp_metadata.supports_add_account) {
    // When there is a single IDP and a single account to show and the IDP does
    // not support adding an account, we can use the single account UI.
    state_ = GetDialogType() == DialogType::MODAL ? State::SINGLE_ACCOUNT_PICKER
                                                  : State::REQUEST_PERMISSION;
    account_selection_view_->ShowSingleAccountConfirmDialog(
        top_frame_for_display_, iframe_for_display_,
        idp_display_data_list_[0].accounts[0], idp_display_data_list_[0],
        /*show_back_button=*/false);
  } else {
    state_ = State::MULTI_ACCOUNT_PICKER;
    account_selection_view_->ShowMultiAccountPicker(idp_display_data_list_);
  }

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return;
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
    if (is_web_contents_visible_) {
      input_protector_->VisibilityChanged(true);
      GetDialogWidget()->Show();
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
}

void FedCmAccountSelectionView::OnAccountsDisplayed() {
  delegate_->OnAccountsDisplayed();
}

void FedCmAccountSelectionView::ShowFailureDialog(
    const std::string& top_frame_etld_plus_one,
    const std::optional<std::string>& iframe_etld_plus_one,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata) {
  state_ = State::IDP_SIGNIN_STATUS_MISMATCH;

  // TODO(crbug.com/1518356): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = false;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead.
  if (account_selection_view_ && rp_mode == blink::mojom::RpMode::kButton &&
      !has_modal_support) {
    ResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  top_frame_for_display_ = base::UTF8ToUTF16(top_frame_etld_plus_one);
  iframe_for_display_ = iframe_etld_plus_one
                            ? std::make_optional<std::u16string>(
                                  base::UTF8ToUTF16(*iframe_etld_plus_one))
                            : std::nullopt;
  if (create_view) {
    account_selection_view_ =
        CreateAccountSelectionView(top_frame_for_display_, iframe_for_display_,
                                   base::UTF8ToUTF16(idp_etld_plus_one),
                                   rp_context, rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return;
    }
  }

  account_selection_view_->ShowFailureDialog(
      top_frame_for_display_, iframe_for_display_,
      base::UTF8ToUTF16(idp_etld_plus_one), idp_metadata);

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (create_view || is_modal_closed_but_accounts_fetch_pending_) {
    is_modal_closed_but_accounts_fetch_pending_ = false;
    if (is_web_contents_visible_) {
      input_protector_->VisibilityChanged(true);
      GetDialogWidget()->Show();
    }
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
}

void FedCmAccountSelectionView::ShowErrorDialog(
    const std::string& top_frame_etld_plus_one,
    const std::optional<std::string>& iframe_etld_plus_one,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error) {
  state_ = State::SIGN_IN_ERROR;
  notify_delegate_of_dismiss_ = true;
  std::optional<std::u16string> iframe_etld_plus_one_u16 =
      iframe_etld_plus_one ? std::make_optional<std::u16string>(
                                 base::UTF8ToUTF16(*iframe_etld_plus_one))
                           : std::nullopt;

  // TODO(crbug.com/1518356): Support modal dialogs for all types of FedCM
  // dialogs. This boolean is used to fall back to the bubble dialog where
  // modal is not yet implemented.
  bool has_modal_support = false;

  // If a modal dialog was created previously but there is no modal support for
  // this type of dialog, reset account_selection_view_ to create a bubble
  // dialog instead.
  if (account_selection_view_ && rp_mode == blink::mojom::RpMode::kButton &&
      !has_modal_support) {
    ResetAccountSelectionView();
  }

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ =
        CreateAccountSelectionView(top_frame_for_display_, iframe_for_display_,
                                   base::UTF8ToUTF16(idp_etld_plus_one),
                                   rp_context, rp_mode, has_modal_support);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return;
    }
  }

  account_selection_view_->ShowErrorDialog(
      base::UTF8ToUTF16(top_frame_etld_plus_one), iframe_etld_plus_one_u16,
      base::UTF8ToUTF16(idp_etld_plus_one), idp_metadata, error);

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (create_view && is_web_contents_visible_) {
    GetDialogWidget()->Show();
    input_protector_->VisibilityChanged(true);
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
}

void FedCmAccountSelectionView::ShowLoadingDialog(
    const std::string& top_frame_etld_plus_one,
    const std::string& idp_etld_plus_one,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode) {
  CHECK(rp_mode == blink::mojom::RpMode::kButton);

  state_ = State::LOADING;
  notify_delegate_of_dismiss_ = true;

  bool create_view = !account_selection_view_;
  if (create_view) {
    account_selection_view_ = CreateAccountSelectionView(
        base::UTF8ToUTF16(top_frame_etld_plus_one),
        /*iframe_etld_plus_one=*/std::nullopt,
        base::UTF8ToUTF16(idp_etld_plus_one), rp_context, rp_mode,
        /*has_modal_support=*/true);

    if (!account_selection_view_) {
      delegate_->OnDismiss(DismissReason::kOther);
      return;
    }
  }

  account_selection_view_->ShowLoadingDialog();

  if (!GetDialogWidget()) {
    delegate_->OnDismiss(DismissReason::kOther);
    return;
  }

  // Initialize InputEventActivationProtector to handle potentially unintended
  // input events. Do not override `input_protector_` set by
  // SetInputEventActivationProtectorForTesting().
  if (!input_protector_) {
    input_protector_ = std::make_unique<views::InputEventActivationProtector>();
  }

  if (create_view && is_web_contents_visible_) {
    GetDialogWidget()->Show();
    input_protector_->VisibilityChanged(true);
  }
  // Else:
  // The dialog is not guaranteed to be shown. The dialog will be hidden if the
  // associated web contents are hidden.
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

void FedCmAccountSelectionView::OnVisibilityChanged(
    content::Visibility visibility) {
  is_web_contents_visible_ = visibility == content::Visibility::VISIBLE;
  if (!GetDialogWidget() || popup_window_ ||
      is_modal_closed_but_accounts_fetch_pending_) {
    return;
  }

  if (is_web_contents_visible_) {
    GetDialogWidget()->Show();
    if (accounts_displayed_callback_) {
      std::move(accounts_displayed_callback_).Run();
    }
    GetDialogWidget()->widget_delegate()->SetCanActivate(true);
    // This will protect against potentially unintentional inputs that happen
    // right after the dialog becomes visible again.
    input_protector_->VisibilityChanged(true);
  } else {
    // On Mac, NativeWidgetMac::Activate() ignores the views::Widget visibility.
    // Make the views::Widget non-activatable while it is hidden to prevent the
    // views::Widget from being shown during focus traversal.
    // TODO(crbug.com/1367309): fix the issue on Mac.
    GetDialogWidget()->Hide();
    GetDialogWidget()->widget_delegate()->SetCanActivate(false);
    input_protector_->VisibilityChanged(false);
  }
}

void FedCmAccountSelectionView::PrimaryPageChanged(content::Page& page) {
  // Close the dialog when the user navigates within the same tab.
  Close();
}

void FedCmAccountSelectionView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  int index =
      tab_strip_model->GetIndexOfWebContents(delegate_->GetWebContents());
  // If the WebContents has been moved out of this `tab_strip_model`, close the
  // dialog.
  // TODO(npm): we should change the management logic so that it is
  // possible to move the dialog with the tab, even to a different browser
  // window.
  if (index == TabStripModel::kNoTab && GetDialogWidget()) {
    Close();
  }
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
    const std::u16string& top_frame_etld_plus_one,
    const std::optional<std::u16string>& iframe_etld_plus_one,
    const std::optional<std::u16string>& idp_title,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    bool has_modal_support) {
  content::WebContents* web_contents = delegate_->GetWebContents();
  Browser* browser = chrome::FindBrowserWithTab(web_contents);

  // Reject the API if the browser is not found or its tab strip model does not
  // exist, as we require those to show UI. It is unclear why there are callers
  // attempting FedCM when some of these checks fail.
  if (!browser || !browser->tab_strip_model()) {
    return nullptr;
  }

  browser->tab_strip_model()->AddObserver(this);

  if (rp_mode == blink::mojom::RpMode::kButton && has_modal_support) {
    dialog_type_ = DialogType::MODAL;
    return new AccountSelectionModalView(
        top_frame_etld_plus_one, idp_title, rp_context, web_contents,
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory(),
        this, this);
  }

  dialog_type_ = DialogType::BUBBLE;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor_view = browser_view->contents_web_view();

  return new AccountSelectionBubbleView(
      top_frame_etld_plus_one, iframe_etld_plus_one, idp_title, rp_context,
      web_contents, anchor_view,
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
    const IdentityProviderDisplayData& idp_display_data,
    const ui::Event& event) {
  DCHECK(state_ != State::IDP_SIGNIN_STATUS_MISMATCH);
  DCHECK(state_ != State::AUTO_REAUTHN);

  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  // Return early if the dialog doesn't need to ask for the user's permission to
  // share their id/email/name/picture.
  if (!idp_display_data.request_permission) {
    delegate_->OnAccountSelected(idp_display_data.idp_metadata.config_url,
                                 account);
    return;
  }

  // At this point, we should request permission. If the account is a returning
  // user or if the account is selected from UI which shows the disclosure text,
  // they have already granted permission.
  if (account.login_state != Account::LoginState::kSignUp ||
      state_ == State::REQUEST_PERMISSION) {
    state_ = State::VERIFYING;
    ShowVerifyingSheet(account, idp_display_data);
    return;
  }

  // At this point, the account is a non-returning user. If the dialog is modal,
  // we'd request permission through the request permission dialog.
  if (GetDialogType() == DialogType::MODAL) {
    state_ = State::REQUEST_PERMISSION;
    account_selection_view_->ShowRequestPermissionDialog(
        top_frame_for_display_, account, idp_display_data);
    return;
  }

  // At this point, the account is a non-returning user, the dialog is a bubble
  // and it is a multi account picker, there is no disclosure text on the dialog
  // so we'd request permission through a single account dialog.
  state_ = State::REQUEST_PERMISSION;
  account_selection_view_->ShowSingleAccountConfirmDialog(
      top_frame_for_display_, iframe_for_display_, account, idp_display_data,
      /*show_back_button=*/true);
}

void FedCmAccountSelectionView::OnLinkClicked(LinkType link_type,
                                              const GURL& url,
                                              const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }
  ShowUrl(link_type, url);
}

void FedCmAccountSelectionView::OnBackButtonClicked() {
  // No need to protect input here since back cannot be the first event.
  state_ = State::MULTI_ACCOUNT_PICKER;
  account_selection_view_->ShowMultiAccountPicker(idp_display_data_list_);
}

void FedCmAccountSelectionView::OnCloseButtonClicked(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.CloseVerifySheet.Desktop",
                        state_ == State::VERIFYING);

  // Record the sheet type that the user was closing.
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.ClosedSheetType.Desktop",
                            GetSheetType(), SheetType::COUNT);

  GetDialogWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void FedCmAccountSelectionView::OnLoginToIdP(const GURL& idp_config_url,
                                             const GURL& idp_login_url,
                                             const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  delegate_->OnLoginToIdP(idp_config_url, idp_login_url);
  is_mismatch_continue_clicked_ = true;
  popup_window_state_ =
      PopupWindowResult::kAccountsNotReceivedAndPopupNotClosedByIdp;
  UMA_HISTOGRAM_ENUMERATION("Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
                            MismatchDialogResult::kContinued);
}

void FedCmAccountSelectionView::OnGotIt(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  delegate_->OnDismiss(DismissReason::kGotItButton);
}

void FedCmAccountSelectionView::OnMoreDetails(const ui::Event& event) {
  if (input_protector_->IsPossiblyUnintendedInteraction(event)) {
    return;
  }

  delegate_->OnMoreDetails();
  delegate_->OnDismiss(DismissReason::kMoreDetailsButton);
}

content::WebContents* FedCmAccountSelectionView::ShowModalDialog(
    const GURL& url) {
  if (!popup_window_) {
    popup_window_ = std::make_unique<FedCmModalDialogView>(
        delegate_->GetWebContents(), this);
  }
  input_protector_->VisibilityChanged(false);
  if (GetDialogWidget()) {
    GetDialogWidget()->Hide();
  }
  return popup_window_->ShowPopupWindow(url);
}

void FedCmAccountSelectionView::CloseModalDialog() {
  if (popup_window_) {
    // If the pop-up window is for IDP sign-in (as triggered from the mismatch
    // dialog or the add account button from the account chooser), we do not
    // destroy the bubble widget and wait for the accounts fetch before
    // displaying a dialog.
    // Otherwise if the pop-up window is for AuthZ or error, we destroy the
    // bubble widget and any incoming accounts fetches would not display any
    // dialog.
    // TODO(crbug.com/1479978): Verify if the current behaviour is what we want
    // for AuthZ/error.
    if (state_ == State::IDP_SIGNIN_STATUS_MISMATCH ||
        state_ == State::MULTI_ACCOUNT_PICKER) {
      should_destroy_dialog_widget_ = false;
      is_modal_closed_but_accounts_fetch_pending_ = true;
      idp_close_popup_time_ = base::TimeTicks::Now();
      popup_window_state_ =
          PopupWindowResult::kAccountsNotReceivedAndPopupClosedByIdp;
    }
    popup_window_->ClosePopupWindow();
    popup_window_.reset();
  }

  if (show_accounts_dialog_callback_) {
    std::move(show_accounts_dialog_callback_).Run();
    // `this` might be deleted now, do not access member variables
    // after this point.
  }
}

void FedCmAccountSelectionView::OnPopupWindowDestroyed() {
  if (!should_destroy_dialog_widget_) {
    return;
  }

  // This triggers the OnDismiss call to notify delegate_
  Close();
}

bool FedCmAccountSelectionView::ShowVerifyingSheet(
    const Account& account,
    const IdentityProviderDisplayData& idp_display_data) {
  DCHECK(state_ == State::VERIFYING || state_ == State::AUTO_REAUTHN);
  notify_delegate_of_dismiss_ = false;

  base::WeakPtr<FedCmAccountSelectionView> weak_ptr(
      weak_ptr_factory_.GetWeakPtr());
  delegate_->OnAccountSelected(idp_display_data.idp_metadata.config_url,
                               account);
  // AccountSelectionView::Delegate::OnAccountSelected() might delete this.
  // See https://crbug.com/1393650 for details.
  if (!weak_ptr) {
    return false;
  }

  const std::u16string title =
      state_ == State::AUTO_REAUTHN
          ? l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE_AUTO_REAUTHN)
          : l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE);
  account_selection_view_->ShowVerifyingSheet(account, idp_display_data, title);
  return true;
}

FedCmAccountSelectionView::SheetType FedCmAccountSelectionView::GetSheetType() {
  switch (state_) {
    case State::IDP_SIGNIN_STATUS_MISMATCH:
      return SheetType::SIGN_IN_TO_IDP_STATIC;

    case State::SINGLE_ACCOUNT_PICKER:
    case State::MULTI_ACCOUNT_PICKER:
    case State::REQUEST_PERMISSION:
      return SheetType::ACCOUNT_SELECTION;

    case State::VERIFYING:
      return SheetType::VERIFYING;

    case State::AUTO_REAUTHN:
      return SheetType::AUTO_REAUTHN;

    case State::SIGN_IN_ERROR:
      return SheetType::SIGN_IN_ERROR;

    case State::LOADING:
      return SheetType::LOADING;

    default:
      NOTREACHED_NORETURN();
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

  ResetAccountSelectionView();
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

void FedCmAccountSelectionView::ResetAccountSelectionView() {
  account_selection_view_->CloseDialog();
  account_selection_view_ = nullptr;
  TabStripModelObserver::StopObservingAll(this);
}
