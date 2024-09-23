// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"

#include <memory>
#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/reauth_tab_helper.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/signin/signin_modal_dialog.h"
#include "chrome/browser/ui/webui/signin/signin_reauth_ui.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/gaia_urls.h"

namespace {

class ReauthWebContentsObserver : public content::WebContentsObserver {
 public:
  ReauthWebContentsObserver(content::WebContents* web_contents,
                            SigninReauthViewController* delegate);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  const raw_ptr<SigninReauthViewController> delegate_;
};

ReauthWebContentsObserver::ReauthWebContentsObserver(
    content::WebContents* web_contents,
    SigninReauthViewController* delegate)
    : WebContentsObserver(web_contents), delegate_(delegate) {}

void ReauthWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  delegate_->OnGaiaReauthPageNavigated();
}

}  // namespace

SigninReauthViewController::SigninReauthViewController(
    Browser* browser,
    const CoreAccountId& account_id,
    signin_metrics::ReauthAccessPoint access_point,
    base::OnceClosure on_close_callback,
    base::OnceCallback<void(signin::ReauthResult)> reauth_callback)
    : SigninModalDialog(std::move(on_close_callback)),
      browser_(browser),
      account_id_(account_id),
      access_point_(access_point),
      reauth_callback_(std::move(reauth_callback)) {
  // Show the confirmation dialog unconditionally for now. We may decide to only
  // show it in some cases in the future.
  ShowReauthConfirmationDialog();

  // Navigate to the Gaia reauth challenge page in background.
  reauth_web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          browser_->profile(),
          content::SiteInstance::Create(browser_->profile())));

  // To allow passing encryption keys during interactions with the page,
  // instantiate TrustedVaultEncryptionKeysTabHelper.
  TrustedVaultEncryptionKeysTabHelper::CreateForWebContents(
      reauth_web_contents_.get());

  const GURL& reauth_url = GaiaUrls::GetInstance()->reauth_url();
  reauth_web_contents_->GetController().LoadURL(
      reauth_url, content::Referrer(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  signin::ReauthTabHelper::CreateForWebContents(
      reauth_web_contents_.get(), reauth_url,
      base::BindOnce(&SigninReauthViewController::OnGaiaReauthPageComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  reauth_web_contents_observer_ = std::make_unique<ReauthWebContentsObserver>(
      reauth_web_contents_.get(), this);
}

SigninReauthViewController::~SigninReauthViewController() {
  for (auto& observer : observer_list_) {
    observer.OnReauthControllerDestroyed();
  }
}

void SigninReauthViewController::CloseModalDialog() {
  CompleteReauth(signin::ReauthResult::kCancelled);
}

void SigninReauthViewController::ResizeNativeView(int height) {
  NOTIMPLEMENTED();
}

content::WebContents*
SigninReauthViewController::GetModalDialogWebContentsForTesting() {
  // If the dialog is displayed, return its WebContents.
  if (dialog_delegate_) {
    return dialog_delegate_->GetWebContents();
  }

  // Return contents of the SAML flow, if exist.
  return raw_reauth_web_contents_;
}

void SigninReauthViewController::OnModalDialogClosed() {
  DCHECK(
      dialog_delegate_observation_.IsObservingSource(dialog_delegate_.get()));
  dialog_delegate_observation_.Reset();
  dialog_delegate_ = nullptr;

  DCHECK(ui_state_ == UIState::kConfirmationDialog ||
         ui_state_ == UIState::kGaiaReauthDialog);
  UserAction action = ui_state_ == UIState::kConfirmationDialog
                          ? UserAction::kCloseConfirmationDialog
                          : UserAction::kCloseGaiaReauthDialog;
  signin_ui_util::RecordTransactionalReauthUserAction(access_point_, action);

  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

void SigninReauthViewController::OnReauthConfirmed(
    sync_pb::UserConsentTypes::AccountPasswordsConsent consent) {
  if (user_confirmed_reauth_) {
    return;
  }

  // Cache the consent. It will be actually recorded later, in CompleteReauth(),
  // if the user successfully completed the reauth.
  consent_ = consent;

  user_confirmed_reauth_ = true;
  OnStateChanged();
}

void SigninReauthViewController::OnReauthDismissed() {
  RecordClickOnce(UserAction::kClickCancelButton);
  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

void SigninReauthViewController::OnGaiaReauthPageNavigated() {
  if (gaia_reauth_page_state_ >= GaiaReauthPageState::kNavigated) {
    return;
  }

  signin::ReauthTabHelper* tab_helper = GetReauthTabHelper();
  DCHECK(tab_helper);
  OnGaiaReauthTypeDetermined(tab_helper->is_within_reauth_origin()
                                 ? GaiaReauthType::kEmbeddedFlow
                                 : GaiaReauthType::kSAMLFlow);
  gaia_reauth_page_state_ = GaiaReauthPageState::kNavigated;
  OnStateChanged();
}

void SigninReauthViewController::OnGaiaReauthPageComplete(
    signin::ReauthResult result) {
  // Should be called only once.
  DCHECK(gaia_reauth_page_state_ < GaiaReauthPageState::kDone);
  DCHECK(!gaia_reauth_page_result_);
  // |kNavigated| state will be skipped if the first navigation completes Gaia
  // reauth.
  if (gaia_reauth_page_state_ < GaiaReauthPageState::kNavigated) {
    OnGaiaReauthTypeDetermined(GaiaReauthType::kAutoApproved);
  }
  gaia_reauth_page_state_ = GaiaReauthPageState::kDone;
  gaia_reauth_page_result_ = result;

  if (ui_state_ == UIState::kGaiaReauthDialog ||
      ui_state_ == UIState::kGaiaReauthTab) {
    std::optional<UserAction> action;
    if (gaia_reauth_page_result_ == signin::ReauthResult::kSuccess) {
      action = UserAction::kPassGaiaReauth;
    }
    if (gaia_reauth_page_result_ == signin::ReauthResult::kDismissedByUser) {
      action = ui_state_ == UIState::kGaiaReauthDialog
                   ? UserAction::kCloseGaiaReauthDialog
                   : UserAction::kCloseGaiaReauthTab;
    }

    if (action) {
      signin_ui_util::RecordTransactionalReauthUserAction(access_point_,
                                                          *action);
    }
  }

  OnStateChanged();
}

void SigninReauthViewController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SigninReauthViewController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SigninReauthViewController::CompleteReauth(signin::ReauthResult result) {
  signin::ReauthTabHelper* tab_helper = GetReauthTabHelper();
  if (tab_helper && tab_helper->has_last_committed_error_page() &&
      result != signin::ReauthResult::kSuccess &&
      (ui_state_ == UIState::kGaiaReauthDialog ||
       ui_state_ == UIState::kGaiaReauthTab)) {
    // Override a non-successful result with |kLoadFailed| if the error page was
    // last displayed to the user.
    result = signin::ReauthResult::kLoadFailed;
  }

  if (dialog_delegate_) {
    DCHECK(
        dialog_delegate_observation_.IsObservingSource(dialog_delegate_.get()));
    dialog_delegate_observation_.Reset();
    dialog_delegate_->CloseModalSignin();
    dialog_delegate_ = nullptr;
  }

  if (raw_reauth_web_contents_) {
    if (!raw_reauth_web_contents_->IsBeingDestroyed()) {
      raw_reauth_web_contents_->ClosePage();
    }
    raw_reauth_web_contents_ = nullptr;
  }

  if (result == signin::ReauthResult::kSuccess) {
    CHECK(consent_.has_value());
    ConsentAuditorFactory::GetForProfile(browser_->profile())
        ->RecordAccountPasswordsConsent(account_id_, *consent_);
  }

  signin_ui_util::RecordTransactionalReauthResult(access_point_, result);
  if (reauth_callback_) {
    std::move(reauth_callback_).Run(result);
  }

  // NotifyModalDialogClosed() will destroy the current instance.
  // We cannot destroy `reauth_web_contents_` right now because this function
  // can be triggered from `reauth_web_contents_`s observer method.
  content::GetUIThreadTaskRunner({})->DeleteSoon(
      FROM_HERE, std::move(reauth_web_contents_));

  NotifyModalDialogClosed();
}

void SigninReauthViewController::OnStateChanged() {
  if (user_confirmed_reauth_ &&
      gaia_reauth_page_state_ == GaiaReauthPageState::kNavigated) {
    RecordClickOnce(UserAction::kClickNextButton);
    ShowGaiaReauthPage();
    return;
  }

  if (user_confirmed_reauth_ &&
      gaia_reauth_page_state_ == GaiaReauthPageState::kDone) {
    DCHECK(gaia_reauth_page_result_);
    RecordClickOnce(UserAction::kClickConfirmButton);
    CompleteReauth(*gaia_reauth_page_result_);
    return;
  }
}

void SigninReauthViewController::OnGaiaReauthTypeDetermined(
    GaiaReauthType reauth_type) {
  DCHECK_EQ(gaia_reauth_type_, GaiaReauthType::kUnknown);
  DCHECK_NE(reauth_type, GaiaReauthType::kUnknown);
  gaia_reauth_type_ = reauth_type;
  for (auto& observer : observer_list_) {
    observer.OnGaiaReauthTypeDetermined(reauth_type);
  }
}

void SigninReauthViewController::RecordClickOnce(UserAction click_action) {
  if (has_recorded_click_) {
    return;
  }

  signin_ui_util::RecordTransactionalReauthUserAction(access_point_,
                                                      click_action);
  has_recorded_click_ = true;
}

signin::ReauthTabHelper* SigninReauthViewController::GetReauthTabHelper() {
  content::WebContents* web_contents = reauth_web_contents_
                                           ? reauth_web_contents_.get()
                                           : raw_reauth_web_contents_.get();
  if (!web_contents) {
    return nullptr;
  }

  return signin::ReauthTabHelper::FromWebContents(web_contents);
}

void SigninReauthViewController::ShowReauthConfirmationDialog() {
  DCHECK_EQ(ui_state_, UIState::kNone);
  ui_state_ = UIState::kConfirmationDialog;
  dialog_delegate_ =
      SigninViewControllerDelegate::CreateReauthConfirmationDelegate(
          browser_, account_id_, access_point_);
  dialog_delegate_observation_.Observe(dialog_delegate_.get());

  SigninReauthUI* web_dialog_ui = dialog_delegate_->GetWebContents()
                                      ->GetWebUI()
                                      ->GetController()
                                      ->GetAs<SigninReauthUI>();
  web_dialog_ui->InitializeMessageHandlerWithReauthController(this);
}

void SigninReauthViewController::ShowGaiaReauthPage() {
  if (gaia_reauth_type_ == GaiaReauthType::kEmbeddedFlow) {
    ShowGaiaReauthPageInDialog();
  } else {
    // This corresponds to a SAML account.
    DCHECK_EQ(gaia_reauth_type_, GaiaReauthType::kSAMLFlow);
    ShowGaiaReauthPageInNewTab();
  }

  for (auto& observer : observer_list_) {
    observer.OnGaiaReauthPageShown();
  }
}

void SigninReauthViewController::ShowGaiaReauthPageInDialog() {
  DCHECK_EQ(ui_state_, UIState::kConfirmationDialog);
  ui_state_ = UIState::kGaiaReauthDialog;
  dialog_delegate_->SetWebContents(reauth_web_contents_.get());
}

void SigninReauthViewController::ShowGaiaReauthPageInNewTab() {
  DCHECK_EQ(ui_state_, UIState::kConfirmationDialog);
  ui_state_ = UIState::kGaiaReauthTab;
  // Remove the observer to not trigger OnModalDialogClosed() that will abort
  // the reauth flow.
  DCHECK(
      dialog_delegate_observation_.IsObservingSource(dialog_delegate_.get()));
  dialog_delegate_observation_.Reset();
  dialog_delegate_->CloseModalSignin();
  dialog_delegate_ = nullptr;

  raw_reauth_web_contents_ = reauth_web_contents_.get();
  NavigateParams nav_params(browser_, std::move(reauth_web_contents_));
  nav_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  nav_params.trusted_source = false;
  nav_params.user_gesture = true;
  nav_params.tabstrip_add_types |= AddTabTypes::ADD_INHERIT_OPENER;
  Navigate(&nav_params);
}
