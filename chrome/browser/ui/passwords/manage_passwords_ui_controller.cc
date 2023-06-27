// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller_impl.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller_impl.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_saver_impl.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/move_password_to_account_store_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

using password_manager::MovePasswordToAccountStoreHelper;
using password_manager::PasswordFormManagerForUI;

int ManagePasswordsUIController::save_fallback_timeout_in_seconds_ = 90;

namespace {

password_manager::PasswordStoreInterface* GetProfilePasswordStore(
    content::WebContents* web_contents) {
  return PasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStoreInterface* GetAccountPasswordStore(
    content::WebContents* web_contents) {
  return AccountPasswordStoreFactory::GetForProfile(
             Profile::FromBrowserContext(web_contents->GetBrowserContext()),
             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

std::vector<std::unique_ptr<password_manager::PasswordForm>> CopyFormVector(
    const std::vector<std::unique_ptr<password_manager::PasswordForm>>& forms) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> result(
      forms.size());
  for (size_t i = 0; i < forms.size(); ++i)
    result[i] = std::make_unique<password_manager::PasswordForm>(*forms[i]);
  return result;
}

const password_manager::InteractionsStats* FindStatsByUsername(
    base::span<const password_manager::InteractionsStats> stats,
    const std::u16string& username) {
  auto it = base::ranges::find(
      stats, username, &password_manager::InteractionsStats::username_value);
  return it == stats.end() ? nullptr : &*it;
}

void MaybeShowPasswordManagerShortcutIPH(Browser* browser) {
  // Don't show IPH if shortcut can't be created.
  if (!web_app::AreWebAppsEnabled(browser->profile())) {
    return;
  }
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHPasswordManagerShortcutFeature);
}

}  // namespace

ManagePasswordsUIController::ManagePasswordsUIController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ManagePasswordsUIController>(*web_contents) {
  passwords_data_.set_client(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  password_manager::PasswordStoreInterface* profile_password_store =
      GetProfilePasswordStore(web_contents);
  if (profile_password_store)
    profile_password_store->AddObserver(this);
  password_manager::PasswordStoreInterface* account_password_store =
      GetAccountPasswordStore(web_contents);
  if (account_password_store)
    account_password_store->AddObserver(this);
}

ManagePasswordsUIController::~ManagePasswordsUIController() = default;

void ManagePasswordsUIController::OnPasswordSubmitted(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  DestroyPopups();
  save_fallback_timer_.Stop();
  passwords_data_.OnPendingPassword(std::move(form_manager));
  if (!IsSavingPromptBlockedExplicitlyOrImplicitly()) {
    bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  }
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnUpdatePasswordSubmitted(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  DestroyPopups();
  save_fallback_timer_.Stop();
  passwords_data_.OnUpdatePassword(std::move(form_manager));
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnShowManualFallbackForSaving(
    std::unique_ptr<PasswordFormManagerForUI> form_manager,
    bool has_generated_password,
    bool is_update) {
  DestroyPopups();
  if (has_generated_password)
    passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  else if (is_update)
    passwords_data_.OnUpdatePassword(std::move(form_manager));
  else
    passwords_data_.OnPendingPassword(std::move(form_manager));
  UpdateBubbleAndIconVisibility();
  save_fallback_timer_.Start(
      FROM_HERE, GetTimeoutForSaveFallback(), this,
      &ManagePasswordsUIController::OnHideManualFallbackForSaving);
}

void ManagePasswordsUIController::OnHideManualFallbackForSaving() {
  if (passwords_data_.state() != password_manager::ui::PENDING_PASSWORD_STATE &&
      passwords_data_.state() !=
          password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      passwords_data_.state() != password_manager::ui::CONFIRMATION_STATE) {
    return;
  }
  // Don't hide the fallback if the bubble is open.
  if (IsShowingBubble())
    return;

  save_fallback_timer_.Stop();

  ClearPopUpFlagForBubble();
  if (passwords_data_.GetCurrentForms().empty())
    passwords_data_.OnInactive();
  else
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);

  UpdateBubbleAndIconVisibility();
}

bool ManagePasswordsUIController::OnChooseCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>
        local_credentials,
    const url::Origin& origin,
    ManagePasswordsState::CredentialsCallback callback) {
  DCHECK(!local_credentials.empty());
  if (!HasBrowserWindow())
    return false;
  // Delete any existing window from the screen.
  dialog_controller_.reset();
  // If |local_credentials| contains PSL matches they shouldn't be propagated to
  // the state (unless they are also web affiliations) because PSL matches
  // aren't saved for current page. This logic is implemented here because
  // Android uses ManagePasswordsState as a data source for account chooser.
  CredentialManagerDialogController::FormsVector locals;
  if (password_manager_util::GetMatchType(*local_credentials[0]) !=
      password_manager_util::GetLoginMatchType::kPSL) {
    locals = CopyFormVector(local_credentials);
  }
  passwords_data_.OnRequestCredentials(std::move(locals), origin);
  passwords_data_.set_credentials_callback(std::move(callback));
  auto* raw_controller = new CredentialManagerDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), this);
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowAccountChooser(CreateAccountChooser(raw_controller),
                                     std::move(local_credentials));
  UpdateBubbleAndIconVisibility();
  return true;
}

void ManagePasswordsUIController::OnAutoSignin(
    std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
    const url::Origin& origin) {
  DCHECK(!local_forms.empty());
  DestroyPopups();
  passwords_data_.OnAutoSignin(std::move(local_forms), origin);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPromptEnableAutoSignin() {
  // Both the account chooser and the previous prompt shouldn't be closed.
  if (dialog_controller_)
    return;
  auto* raw_controller = new CredentialManagerDialogControllerImpl(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), this);
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowAutosigninPrompt(CreateAutoSigninPrompt(raw_controller));
}

void ManagePasswordsUIController::OnAutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> form_manager) {
  DestroyPopups();
  save_fallback_timer_.Stop();
  passwords_data_.OnAutomaticPasswordSave(std::move(form_manager));
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordAutofilled(
    const std::vector<const password_manager::PasswordForm*>& password_forms,
    const url::Origin& origin,
    const std::vector<const password_manager::PasswordForm*>*
        federated_matches) {
  // To change to managed state only when the managed state is more important
  // for the user that the current state.
  if (passwords_data_.state() == password_manager::ui::INACTIVE_STATE ||
      passwords_data_.state() == password_manager::ui::MANAGE_STATE) {
    ClearPopUpFlagForBubble();
    passwords_data_.OnPasswordAutofilled(password_forms, origin,
                                         federated_matches);
    // Don't close the existing bubble. Update the icon later.
    if (bubble_status_ == BubbleStatus::SHOWN) {
      bubble_status_ = BubbleStatus::SHOWN_PENDING_ICON_UPDATE;
    }
    if (bubble_status_ != BubbleStatus::SHOWN_PENDING_ICON_UPDATE) {
      UpdateBubbleAndIconVisibility();
    }

    if (GetState() == password_manager::ui::MANAGE_STATE) {
      if (Browser* browser =
              chrome::FindBrowserWithWebContents(web_contents())) {
        if (browser->tab_strip_model()->GetActiveWebContents() ==
            web_contents()) {
          const bool has_non_empty_note =
              !base::ranges::all_of(GetCurrentForms(), &std::u16string::empty,
                                    &password_manager::PasswordForm::
                                        GetNoteWithEmptyUniqueDisplayName);
          if (has_non_empty_note &&
              browser->window()->MaybeShowFeaturePromo(
                  feature_engagement::
                      kIPHPasswordsManagementBubbleDuringSigninFeature)) {
            return;
          }
          MaybeShowPasswordManagerShortcutIPH(browser);
        }
      }
    }
  }
}

void ManagePasswordsUIController::OnCredentialLeak(
    const password_manager::CredentialLeakType leak_type,
    const GURL& url,
    const std::u16string& username) {
  // Existing dialog shouldn't be closed.
  if (dialog_controller_)
    return;

  // Hide the manage passwords bubble if currently shown.
  if (IsShowingBubble())
    HidePasswordBubble();
  else
    ClearPopUpFlagForBubble();

  auto* raw_controller = new CredentialLeakDialogControllerImpl(
      this, leak_type, url, username,
      std::make_unique<
          password_manager::metrics_util::LeakDialogMetricsRecorder>(
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId(),
          password_manager::GetLeakDialogType(leak_type)));
  dialog_controller_.reset(raw_controller);
  raw_controller->ShowCredentialLeakPrompt(
      CreateCredentialLeakPrompt(raw_controller));
}

void ManagePasswordsUIController::OnShowMoveToAccountBubble(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move) {
  base::UmaHistogramEnumeration(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kSuccessfulLoginWithProfileStorePassword);
  if (!GetPasswordFeatureManager()->IsOptedInForAccountStorage())
    GetPasswordFeatureManager()->RecordMoveOfferedToNonOptedInUser();
  passwords_data_.OnPasswordMovable(std::move(form_to_move));
  // TODO(crbug.com/1100814): Add smartness like OnPasswordSubmitted?
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnBiometricAuthenticationForFilling(
    PrefService* prefs) {
  // Existing dialog shouldn't be closed.
  if (dialog_controller_) {
    return;
  }
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  const std::string promo_shown_counter =
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter;
  // Checking GetIfBiometricAuthenticationPromoWasShown() prevents from
  // displaying multiple prompts on the same tab (eg. when there are multiple
  // password forms).
  if (was_biometric_authentication_for_filling_promo_shown_ ||
      prefs->GetBoolean(
          password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo) ||
      prefs->GetInteger(promo_shown_counter) >=
          kMaxNumberOfTimesBiometricAuthForFillingPromoWillBeShown ||
      prefs->GetBoolean(
          password_manager::prefs::kBiometricAuthenticationBeforeFilling)) {
    return;
  }
  prefs->SetInteger(promo_shown_counter,
                    prefs->GetInteger(promo_shown_counter) + 1);

  passwords_data_.TransitionToState(
      password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  was_biometric_authentication_for_filling_promo_shown_ = true;
  UpdateBubbleAndIconVisibility();
#else
  NOTIMPLEMENTED();
#endif
}

void ManagePasswordsUIController::ShowBiometricActivationConfirmation() {
  // Existing dialog shouldn't be closed.
  if (dialog_controller_) {
    return;
  }
  passwords_data_.TransitionToState(
      password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnBiometricAuthBeforeFillingDeclined() {
  CHECK(!dialog_controller_);
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NotifyUnsyncedCredentialsWillBeDeleted(
    std::vector<password_manager::PasswordForm> unsynced_credentials) {
  passwords_data_.ProcessUnsyncedCredentialsWillBeDeleted(
      std::move(unsynced_credentials));
  DCHECK(GetState() ==
         password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnLoginsChanged(
    password_manager::PasswordStoreInterface* /*store*/,
    const password_manager::PasswordStoreChangeList& changes) {
  password_manager::ui::State current_state = GetState();
  passwords_data_.ProcessLoginsChanged(changes);
  if (current_state != GetState()) {
    ClearPopUpFlagForBubble();
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
}

void ManagePasswordsUIController::UpdateIconAndBubbleState(
    ManagePasswordsIconView* icon) {
  if (IsAutomaticallyOpeningBubble()) {
    DCHECK(!dialog_controller_);
    // This will detach any existing bubble so OnBubbleHidden() isn't called.
    weak_ptr_factory_.InvalidateWeakPtrs();
    // We must display the icon before showing the bubble, as the bubble would
    // be otherwise unanchored.
    icon->SetState(GetState());
    ShowBubbleWithoutUserInteraction();
    // If the bubble appeared then the status is updated in OnBubbleShown().
    ClearPopUpFlagForBubble();
  } else {
    password_manager::ui::State state = GetState();
    // The dialog should hide the icon.
    if (dialog_controller_ &&
        state == password_manager::ui::CREDENTIAL_REQUEST_STATE)
      state = password_manager::ui::INACTIVE_STATE;
    icon->SetState(state);
  }
}

base::WeakPtr<PasswordsModelDelegate>
ManagePasswordsUIController::GetModelDelegateProxy() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* ManagePasswordsUIController::GetWebContents() const {
  return web_contents();
}

url::Origin ManagePasswordsUIController::GetOrigin() const {
  return passwords_data_.origin();
}

password_manager::PasswordFormMetricsRecorder*
ManagePasswordsUIController::GetPasswordFormMetricsRecorder() {
  // The form manager may be null for example for auto sign-in toasts of the
  // credential manager API.
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager ? form_manager->GetMetricsRecorder() : nullptr;
}

password_manager::PasswordFeatureManager*
ManagePasswordsUIController::GetPasswordFeatureManager() {
  password_manager::PasswordManagerClient* client = passwords_data_.client();
  return client->GetPasswordFeatureManager();
}

password_manager::ui::State ManagePasswordsUIController::GetState() const {
  return passwords_data_.state();
}

const password_manager::PasswordForm&
ManagePasswordsUIController::GetPendingPassword() const {
  if (GetState() == password_manager::ui::AUTO_SIGNIN_STATE)
    return *GetCurrentForms()[0];

  DCHECK(GetState() == password_manager::ui::PENDING_PASSWORD_STATE ||
         GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         GetState() == password_manager::ui::CONFIRMATION_STATE ||
         GetState() == password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE)
      << GetState();
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager->GetPendingCredentials();
}

password_manager::metrics_util::CredentialSourceType
ManagePasswordsUIController::GetCredentialSource() const {
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return form_manager
             ? form_manager->GetCredentialSource()
             : password_manager::metrics_util::CredentialSourceType::kUnknown;
}

const std::vector<password_manager::PasswordForm>&
ManagePasswordsUIController::GetUnsyncedCredentials() const {
  return passwords_data_.unsynced_credentials();
}

const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
ManagePasswordsUIController::GetCurrentForms() const {
  return passwords_data_.GetCurrentForms();
}

const password_manager::InteractionsStats*
ManagePasswordsUIController::GetCurrentInteractionStats() const {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  return FindStatsByUsername(
      form_manager->GetInteractionsStats(),
      form_manager->GetPendingCredentials().username_value);
}

size_t ManagePasswordsUIController::GetTotalNumberCompromisedPasswords() const {
  DCHECK(GetState() == password_manager::ui::PASSWORD_UPDATED_SAFE_STATE ||
         GetState() == password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
  return post_save_compromised_helper_->compromised_count();
}

bool ManagePasswordsUIController::DidAuthForAccountStoreOptInFail() const {
  return passwords_data_.auth_for_account_storage_opt_in_failed();
}

bool ManagePasswordsUIController::BubbleIsManualFallbackForSaving() const {
  return save_fallback_timer_.IsRunning();
}

void ManagePasswordsUIController::OnBubbleShown() {
  bubble_status_ = BubbleStatus::SHOWN;
}

void ManagePasswordsUIController::OnBubbleHidden() {
  bool update_icon =
      (bubble_status_ == BubbleStatus::SHOWN_PENDING_ICON_UPDATE);
  bubble_status_ = BubbleStatus::NOT_SHOWN;
  if (GetState() == password_manager::ui::CONFIRMATION_STATE ||
      GetState() == password_manager::ui::AUTO_SIGNIN_STATE ||
      GetState() ==
          password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE ||
      GetState() == password_manager::ui::PASSWORD_UPDATED_SAFE_STATE ||
      GetState() == password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX) {
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    update_icon = true;
  }
  if (update_icon)
    UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnNoInteraction() {
  if (GetState() != password_manager::ui::PENDING_PASSWORD_UPDATE_STATE &&
      GetState() != password_manager::ui::PENDING_PASSWORD_STATE) {
    // Do nothing if the state was changed. It can happen for example when the
    // bubble is active and a page navigation happens.
    return;
  }
  bool is_update =
      GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE;
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNoInteraction(is_update);
}

void ManagePasswordsUIController::OnNopeUpdateClicked() {
  password_manager::PasswordFormManagerForUI* form_manager =
      passwords_data_.form_manager();
  DCHECK(form_manager);
  form_manager->OnNopeUpdateClicked();
}

void ManagePasswordsUIController::NeverSavePassword() {
  DCHECK_EQ(password_manager::ui::PENDING_PASSWORD_STATE, GetState());
  passwords_data_.form_manager()->OnNeverClicked();
  ClearPopUpFlagForBubble();
  if (passwords_data_.GetCurrentForms().empty())
    passwords_data_.OnInactive();
  else
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnPasswordsRevealed() {
  DCHECK(passwords_data_.form_manager());
  passwords_data_.form_manager()->OnPasswordsRevealed();
}

void ManagePasswordsUIController::SavePassword(const std::u16string& username,
                                               const std::u16string& password) {
  UpdatePasswordFormUsernameAndPassword(username, password,
                                        passwords_data_.form_manager());

  if (auto* sentiment_service =
          TrustSafetySentimentServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_contents()->GetBrowserContext()))) {
    sentiment_service->SavedPassword();
  }

  if (GetPasswordFormMetricsRecorder() && BubbleIsManualFallbackForSaving()) {
    GetPasswordFormMetricsRecorder()->RecordDetailedUserAction(
        password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
            kTriggeredManualFallbackForSaving);
  }
  save_fallback_timer_.Stop();
  passwords_data_.form_manager()->Save();

  // If we just saved a password to the account store, notify the IPH tracker
  // about it (so it can decide not to show the IPH again).
  if (GetPasswordFeatureManager()->GetDefaultPasswordStore() ==
      password_manager::PasswordForm::Store::kAccountStore) {
    feature_engagement::TrackerFactory::GetForBrowserContext(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()))
        ->NotifyEvent("passwords_account_storage_used");
  }

  post_save_compromised_helper_ =
      std::make_unique<password_manager::PostSaveCompromisedHelper>(
          passwords_data_.form_manager()->GetInsecureCredentials(), username);
  post_save_compromised_helper_->AnalyzeLeakedCredentials(
      passwords_data_.client()->GetProfilePasswordStore(),
      passwords_data_.client()->GetAccountPasswordStore(),
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs(),
      base::BindOnce(
          &ManagePasswordsUIController::OnTriggerPostSaveCompromisedBubble,
          weak_ptr_factory_.GetWeakPtr()));

  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  // The icon is to be updated after the bubble (either "Save password" or "Sign
  // in to Chrome") is closed.
  bubble_status_ = BubbleStatus::SHOWN_PENDING_ICON_UPDATE;
  if (Browser* browser = chrome::FindBrowserWithWebContents(web_contents())) {
    browser->window()->GetAutofillBubbleHandler()->OnPasswordSaved();
    if (browser->window()->MaybeShowFeaturePromo(
            feature_engagement::
                kIPHPasswordsManagementBubbleAfterSaveFeature)) {
      return;
    }
    MaybeShowPasswordManagerShortcutIPH(browser);
  }
}

void ManagePasswordsUIController::SaveUnsyncedCredentialsInProfileStore(
    const std::vector<password_manager::PasswordForm>& selected_credentials) {
  auto profile_store_form_saver =
      std::make_unique<password_manager::FormSaverImpl>(
          passwords_data_.client()->GetProfilePasswordStore());
  for (const password_manager::PasswordForm& form : selected_credentials) {
    // Only newly-saved or newly-updated credentials can be unsynced. Since
    // conflicts are solved in that process, any entry in the profile store
    // similar to |form| actually contains the same essential information. This
    // means Save() can be safely called here, no password loss happens.
    profile_store_form_saver->Save(form, /*matches=*/{},
                                   /*old_password=*/std::u16string());
  }
  ClearPopUpFlagForBubble();
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::DiscardUnsyncedCredentials() {
  ClearPopUpFlagForBubble();
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::MovePasswordToAccountStore() {
  DCHECK_EQ(GetState(),
            password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE)
      << GetState();
  passwords_data_.form_manager()->MoveCredentialsToAccountStore();
  ClearPopUpFlagForBubble();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::BlockMovingPasswordToAccountStore() {
  DCHECK_EQ(GetState(),
            password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE)
      << GetState();
  passwords_data_.form_manager()->BlockMovingCredentialsToAccountStore();
  ClearPopUpFlagForBubble();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::ChooseCredential(
    const password_manager::PasswordForm& form,
    password_manager::CredentialType credential_type) {
  CHECK(dialog_controller_);
  CHECK_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD,
           credential_type);
  // Copy the argument before destroying the controller. |form| is a member of
  // |dialog_controller_|.
  password_manager::PasswordForm copy_form = form;
  dialog_controller_.reset();
  passwords_data_.ChooseCredential(&copy_form);
  ClearPopUpFlagForBubble();
  passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::NavigateToPasswordManagerSettingsPage(
    password_manager::ManagePasswordsReferrer referrer) {
  NavigateToManagePasswordsPage(
      chrome::FindBrowserWithWebContents(web_contents()), referrer);
}

void ManagePasswordsUIController::NavigateToPasswordManagerAccountDashboard(
    password_manager::ManagePasswordsReferrer referrer) {
  NavigateToGooglePasswordManager(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      referrer);
}

void ManagePasswordsUIController::NavigateToPasswordCheckup(
    password_manager::PasswordCheckReferrer referrer) {
  chrome::ShowPasswordCheck(chrome::FindBrowserWithWebContents(web_contents()));
  password_manager::LogPasswordCheckReferrer(referrer);
}

void ManagePasswordsUIController::EnableSync(const AccountInfo& account) {
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()), account,
      signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE);
}

void ManagePasswordsUIController::OnDialogHidden() {
  dialog_controller_.reset();
  if (GetState() == password_manager::ui::CREDENTIAL_REQUEST_STATE) {
    ClearPopUpFlagForBubble();
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
    UpdateBubbleAndIconVisibility();
  }
}

void ManagePasswordsUIController::OnLeakDialogHidden() {
  dialog_controller_.reset();
  if (GetState() == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    bubble_status_ = BubbleStatus::SHOULD_POP_UP;
    UpdateBubbleAndIconVisibility();
    return;
  }
  if (GetState() == password_manager::ui::PENDING_PASSWORD_STATE) {
    if (!IsSavingPromptBlockedExplicitlyOrImplicitly()) {
      bubble_status_ = BubbleStatus::SHOULD_POP_UP;
    }
    UpdateBubbleAndIconVisibility();
  }
}

bool ManagePasswordsUIController::IsSavingPromptBlockedExplicitlyOrImplicitly()
    const {
  PasswordFormManagerForUI* form_manager = passwords_data_.form_manager();
  DCHECK(form_manager);
  if (form_manager->IsBlocklisted()) {
    return true;
  }

  const password_manager::InteractionsStats* stats =
      GetCurrentInteractionStats();
  const int show_threshold =
      password_bubble_experiment::GetSmartBubbleDismissalThreshold();
  return stats && show_threshold > 0 &&
         stats->dismissal_count >= show_threshold;
}

void ManagePasswordsUIController::AuthenticateUserWithMessage(
    const std::u16string& message,
    AvailabilityCallback callback) {
  if (bypass_user_auth_for_testing_) {
    std::move(callback).Run(true);
    return;
  }
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  std::move(callback).Run(true);
  return;
#else
  base::OnceClosure on_reauth_completed =
      base::BindOnce(&ManagePasswordsUIController::OnReauthCompleted,
                     weak_ptr_factory_.GetWeakPtr());

  CancelAnyOngoingBiometricAuth();
  biometric_authenticator_ = passwords_data_.client()->GetDeviceAuthenticator();
  biometric_authenticator_->AuthenticateWithMessage(
      message, std::move(callback).Then(std::move(on_reauth_completed)));
#endif  // !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
}

void ManagePasswordsUIController::
    AuthenticateUserForAccountStoreOptInAndSavePassword(
        const std::u16string& username,
        const std::u16string& password) {
  password_manager::PasswordManagerClient* client = passwords_data_.client();
  client->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kPasswordSaveBubble,
      base::BindOnce(&ManagePasswordsUIController::
                         FinishSavingPasswordAfterAccountStoreOptInAuth,
                     weak_ptr_factory_.GetWeakPtr(), passwords_data_.origin(),
                     passwords_data_.form_manager(), username, password));
}

void ManagePasswordsUIController::
    AuthenticateUserForAccountStoreOptInAfterSavingLocallyAndMovePassword() {
  DCHECK(GetState() == password_manager::ui::MANAGE_STATE) << GetState();
  // Note: While saving the password locally earlier, the FormManager has been
  // updated with any edits the user made in the Save bubble. So at this point,
  // just using GetPendingCredentials() is safe.
  passwords_data_.client()->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kPasswordSaveLocallyBubble,
      base::BindOnce(&ManagePasswordsUIController::
                         MoveJustSavedPasswordAfterAccountStoreOptIn,
                     weak_ptr_factory_.GetWeakPtr(),
                     passwords_data_.form_manager()->GetPendingCredentials()));
}

void ManagePasswordsUIController::MaybeShowIOSPasswordPromo() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser) {
    return;
  }
  browser->window()->VerifyUserEligibilityIOSPasswordPromoBubble();
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ManagePasswordsUIController::
    AuthenticateUserForAccountStoreOptInAndMovePassword() {
  DCHECK_EQ(GetState(),
            password_manager::ui::CAN_MOVE_PASSWORD_TO_ACCOUNT_STATE)
      << GetState();
  passwords_data_.client()->TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint::kPasswordMoveBubble,
      base::BindOnce(&ManagePasswordsUIController::
                         FinishMovingPasswordAfterAccountStoreOptInAuth,
                     weak_ptr_factory_.GetWeakPtr(),
                     passwords_data_.form_manager()));
}

[[nodiscard]] std::unique_ptr<base::AutoReset<bool>>
ManagePasswordsUIController::BypassUserAuthtForTesting() {
  return std::make_unique<base::AutoReset<bool>>(&bypass_user_auth_for_testing_,
                                                 true);
}

void ManagePasswordsUIController::HidePasswordBubble() {
  if (TabDialogs* tab_dialogs = TabDialogs::FromWebContents(web_contents()))
    tab_dialogs->HideManagePasswordsBubble();
}

void ManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  // If we're not on a "webby" URL (e.g. "chrome://sign-in"), we shouldn't
  // display either the bubble or the icon.
  if (!ChromePasswordManagerClient::CanShowBubbleOnURL(
          web_contents()->GetLastCommittedURL())) {
    ClearPopUpFlagForBubble();
    passwords_data_.OnInactive();
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  browser->window()->UpdatePageActionIcon(PageActionIconType::kManagePasswords);
}

AccountChooserPrompt* ManagePasswordsUIController::CreateAccountChooser(
    CredentialManagerDialogController* controller) {
  return CreateAccountChooserPromptView(controller, web_contents());
}

AutoSigninFirstRunPrompt* ManagePasswordsUIController::CreateAutoSigninPrompt(
    CredentialManagerDialogController* controller) {
  return CreateAutoSigninPromptView(controller, web_contents());
}

CredentialLeakPrompt* ManagePasswordsUIController::CreateCredentialLeakPrompt(
    CredentialLeakDialogController* controller) {
  return CreateCredentialLeakPromptView(controller, web_contents());
}

bool ManagePasswordsUIController::HasBrowserWindow() const {
  return chrome::FindBrowserWithWebContents(web_contents()) != nullptr;
}

void ManagePasswordsUIController::PrimaryPageChanged(content::Page& page) {
  CancelAnyOngoingBiometricAuth();

  // Keep the state if the bubble is currently open or the fallback for saving
  // should be still available.
  if (IsShowingBubble() || save_fallback_timer_.IsRunning()) {
    return;
  }

  // Otherwise, reset the password manager.
  DestroyPopups();
  ClearPopUpFlagForBubble();
  passwords_data_.OnInactive();
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    HidePasswordBubble();
}

// static
base::TimeDelta ManagePasswordsUIController::GetTimeoutForSaveFallback() {
  return base::Seconds(
      ManagePasswordsUIController::save_fallback_timeout_in_seconds_);
}

void ManagePasswordsUIController::ShowBubbleWithoutUserInteraction() {
  DCHECK(IsAutomaticallyOpeningBubble());
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  // Can be zero in the tests.
  if (!browser)
    return;

  chrome::ExecuteCommand(browser, IDC_MANAGE_PASSWORDS_FOR_PAGE);
}

void ManagePasswordsUIController::ClearPopUpFlagForBubble() {
  if (IsAutomaticallyOpeningBubble()) {
    bubble_status_ = BubbleStatus::NOT_SHOWN;
  }
}

void ManagePasswordsUIController::DestroyPopups() {
  HidePasswordBubble();
  if (dialog_controller_ && dialog_controller_->IsShowingAccountChooser()) {
    dialog_controller_.reset();
    passwords_data_.TransitionToState(password_manager::ui::MANAGE_STATE);
  }
}

void ManagePasswordsUIController::WebContentsDestroyed() {
  password_manager::PasswordStoreInterface* profile_password_store =
      GetProfilePasswordStore(web_contents());
  if (profile_password_store)
    profile_password_store->RemoveObserver(this);
  password_manager::PasswordStoreInterface* account_password_store =
      GetAccountPasswordStore(web_contents());
  if (account_password_store)
    account_password_store->RemoveObserver(this);
  HidePasswordBubble();
}

void ManagePasswordsUIController::
    FinishSavingPasswordAfterAccountStoreOptInAuth(
        const url::Origin& origin,
        password_manager::PasswordFormManagerForUI* form_manager,
        const std::u16string& username,
        const std::u16string& password,
        password_manager::PasswordManagerClient::ReauthSucceeded
            reauth_succeeded) {
  passwords_data_.set_auth_for_account_storage_opt_in_failed(!reauth_succeeded);
  if (reauth_succeeded) {
    // Save the password only if it is the same origin and same form manager.
    // Otherwise it can be dangerous (e.g. saving the credentials against
    // another origin).
    if (passwords_data_.origin() == origin &&
        passwords_data_.form_manager() == form_manager) {
      SavePassword(username, password);
    }
    return;
  }
  // If reauth wasn't successful, change to local store and reopen the bubble is
  // the state didn't change.
  GetPasswordFeatureManager()->SetDefaultPasswordStore(
      password_manager::PasswordForm::Store::kProfileStore);
  if (passwords_data_.state() != password_manager::ui::PENDING_PASSWORD_STATE)
    return;
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::OnTriggerPostSaveCompromisedBubble(
    password_manager::PostSaveCompromisedHelper::BubbleType type,
    size_t count_compromised_passwords_) {
  using password_manager::PostSaveCompromisedHelper;
  // If the controller changed the state in the mean time or the Sign-in promo
  // is still open, don't show anything.
  if (passwords_data_.state() != password_manager::ui::MANAGE_STATE ||
      bubble_status_ != BubbleStatus::NOT_SHOWN)
    return;
  password_manager::ui::State state;
  switch (type) {
    case PostSaveCompromisedHelper::BubbleType::kNoBubble:
      post_save_compromised_helper_.reset();
      return;
    case PostSaveCompromisedHelper::BubbleType::kPasswordUpdatedSafeState:
      state = password_manager::ui::PASSWORD_UPDATED_SAFE_STATE;
      break;
    case PostSaveCompromisedHelper::BubbleType::kPasswordUpdatedWithMoreToFix:
      state = password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX;
      break;
  }
  passwords_data_.TransitionToState(state);
  bubble_status_ = BubbleStatus::SHOULD_POP_UP;
  UpdateBubbleAndIconVisibility();
}

void ManagePasswordsUIController::
    FinishMovingPasswordAfterAccountStoreOptInAuth(
        password_manager::PasswordFormManagerForUI* form_manager,
        password_manager::PasswordManagerClient::ReauthSucceeded
            reauth_succeeded) {
  if (!reauth_succeeded || passwords_data_.form_manager() != form_manager) {
    return;
  }
  MovePasswordToAccountStore();
}

void ManagePasswordsUIController::MoveJustSavedPasswordAfterAccountStoreOptIn(
    password_manager::PasswordForm form,
    password_manager::PasswordManagerClient::ReauthSucceeded reauth_succeeded) {
  // Successful opt-in means that the just-saved password should be moved to the
  // account.
  if (reauth_succeeded) {
    // Insert nullptr first to obtain the iterator passed to the callback.
    auto helper_it = move_to_account_store_helpers_.insert(
        move_to_account_store_helpers_.begin(), nullptr);
    // This class owns and thus outlives the helper so base::Unretained is safe.
    *helper_it = std::make_unique<MovePasswordToAccountStoreHelper>(
        form, passwords_data_.client(),
        password_manager::metrics_util::MoveToAccountStoreTrigger::
            kUserOptedInAfterSavingLocally,
        base::BindOnce(
            &ManagePasswordsUIController::
                OnMoveJustSavedPasswordAfterAccountStoreOptInCompleted,
            base::Unretained(this), helper_it));
  } else {
    // Failed or canceled opt-in means the user has (implicitly) chosen to save
    // locally. This is already the default value, but setting it explicitly
    // makes sure the user won't be asked to opt in again (since "store not set"
    // gets interpreted as "first-time save").
    GetPasswordFeatureManager()->SetDefaultPasswordStore(
        password_manager::PasswordForm::Store::kProfileStore);
  }
}

void ManagePasswordsUIController::
    OnMoveJustSavedPasswordAfterAccountStoreOptInCompleted(
        std::list<std::unique_ptr<MovePasswordToAccountStoreHelper>>::iterator
            done_helper_it) {
  move_to_account_store_helpers_.erase(done_helper_it);
}

void ManagePasswordsUIController::OnReauthCompleted() {
  biometric_authenticator_.reset();
}

void ManagePasswordsUIController::CancelAnyOngoingBiometricAuth() {
  if (!biometric_authenticator_)
    return;
  biometric_authenticator_->Cancel(
      device_reauth::DeviceAuthRequester::kTouchToFill);
  biometric_authenticator_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManagePasswordsUIController);
