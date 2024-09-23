// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/branding_buildflags.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"

namespace content {
class WebContents;
}
namespace password_manager {
struct InteractionsStats;
class PasswordFeatureManager;
class PasswordFormMetricsRecorder;
struct PasswordForm;
namespace metrics_util {
enum class CredentialSourceType;
enum class MoveToAccountStoreTrigger;
}  // namespace metrics_util
}  // namespace password_manager

// An interface for ManagePasswordsBubbleModel implemented by
// ManagePasswordsUIController. Allows to retrieve the current state of the tab
// and notify about user actions.
class PasswordsModelDelegate {
 public:
  using AvailabilityCallback = base::OnceCallback<void(bool)>;

  // Returns WebContents* the model is attached to.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns the password_manager::PasswordFormMetricsRecorder that is
  // associated with the PasswordFormManager that governs the password being
  // submitted.
  virtual password_manager::PasswordFormMetricsRecorder*
  GetPasswordFormMetricsRecorder() = 0;

  virtual password_manager::PasswordFeatureManager*
  GetPasswordFeatureManager() = 0;

  // Returns the URL of the site the current forms are retrieved for.
  virtual url::Origin GetOrigin() const = 0;

  // Returns the current tab state.
  virtual password_manager::ui::State GetState() const = 0;

  // Returns the pending password in PENDING_PASSWORD_STATE and
  // PENDING_PASSWORD_UPDATE_STATE, the saved password in
  // SAVE_CONFIRMATION_STATE, the returned credential in AUTO_SIGNIN_STATE.
  virtual const password_manager::PasswordForm& GetPendingPassword() const = 0;

  // Returns unsynced credentials being deleted upon signout.
  virtual const std::vector<password_manager::PasswordForm>&
  GetUnsyncedCredentials() const = 0;

  // Returns the source of the credential to be saved.
  virtual password_manager::metrics_util::CredentialSourceType
  GetCredentialSource() const = 0;

  // Returns current local forms for the current page.
  virtual const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCurrentForms() const = 0;

  // Returns credential for the manage passwords bubble in the single credential
  // mode. Providing a form by this method allows to use the bubble to display
  // arbitrary password form details, not only those from the list of website
  // related credentials. When this method returns `nullopt`, a list of stored
  // credentials for the current origin are displayed in the bubble.
  virtual const std::optional<password_manager::PasswordForm>&
  GetManagePasswordsSingleCredentialDetailsModeCredential() const = 0;

  // For PENDING_PASSWORD_STATE state returns the current statistics for
  // the pending username.
  virtual const password_manager::InteractionsStats*
  GetCurrentInteractionStats() const = 0;

  // For PASSWORD_UPDATED_* return # compromised passwords in the store.
  virtual size_t GetTotalNumberCompromisedPasswords() const = 0;

  // Users need to reauth to their account to opt-in using their password
  // account storage. This method returns whether account auth attempt during
  // the last password save process failed or not.
  virtual bool DidAuthForAccountStoreOptInFail() const = 0;

  // Returns true iff the current bubble is the manual fallback for saving.
  virtual bool BubbleIsManualFallbackForSaving() const = 0;

  // Returns true if GPM pin was created during the most recent passkey creation
  // flow, applicable for PASSKEY_SAVED_CONFIRMATION_STATE only.
  virtual bool GpmPinCreatedDuringRecentPasskeyCreation() const = 0;

  // Called from the model when the bubble is displayed.
  virtual void OnBubbleShown() = 0;

  // Called from the model when the bubble is hidden.
  virtual void OnBubbleHidden() = 0;

  // Called when the user didn't interact with UI.
  virtual void OnNoInteraction() = 0;

  // Called when the user chose not to update password.
  virtual void OnNopeUpdateClicked() = 0;

  // Called from the model when the user chooses to never save passwords.
  virtual void NeverSavePassword() = 0;

  // Called when the passwords are revealed to the user without obfuscation.
  virtual void OnPasswordsRevealed() = 0;

  // Called from the model when the user chooses to save a password. The
  // username and password seen on the ui is sent as a parameter, and
  // handled accordingly if user had edited them.
  virtual void SavePassword(const std::u16string& username,
                            const std::u16string& password) = 0;

  // Called when the user chooses to save locally some of the unsynced
  // credentials that were deleted from the account store on signout.
  virtual void SaveUnsyncedCredentialsInProfileStore(
      const std::vector<password_manager::PasswordForm>&
          selected_credentials) = 0;

  // Called when the user chooses not to save locally the unsynced credentials
  // deleted from the account store on signout (the ones returned by
  // GetUnsyncedCredentials()).
  virtual void DiscardUnsyncedCredentials() = 0;

  // Called from the dialog controller when a user confirms moving the recently
  // used or selected credential to their account store.
  virtual void MovePasswordToAccountStore() = 0;

  // Moves pending password to the account storage.
  virtual void MovePendingPasswordToAccountStoreUsingHelper(
      const password_manager::PasswordForm&,
      password_manager::metrics_util::MoveToAccountStoreTrigger) = 0;

  // Called from the dialog controller when a user rejects moving the recently
  // used credential to their account store.
  virtual void BlockMovingPasswordToAccountStore() = 0;

  // Called from the dialog controller when the user acknowledges that their
  // default password store setting changed.
  virtual void PromptSaveBubbleAfterDefaultStoreChanged() = 0;

  // Called from the dialog controller when the user chooses a credential.
  // Controller can be destroyed inside the method.
  virtual void ChooseCredential(
      const password_manager::PasswordForm& form,
      password_manager::CredentialType credential_type) = 0;

  // Open a new tab, pointing to the password manager settings page.
  virtual void NavigateToPasswordManagerSettingsPage(
      password_manager::ManagePasswordsReferrer referrer) = 0;

  // Open a new tab, pointing to the password manager subpage with the
  // credential details for the `password_domain_name`.
  virtual void NavigateToPasswordDetailsPageInPasswordManager(
      const std::string& password_domain_name,
      password_manager::ManagePasswordsReferrer referrer) = 0;

  // Opens password manager settings page and focuses account store toggle.
  virtual void NavigateToPasswordManagerSettingsAccountStoreToggle(
      password_manager::ManagePasswordsReferrer referrer) = 0;

  // Open a new tab, pointing to the password check in the settings page.
  virtual void NavigateToPasswordCheckup(
      password_manager::PasswordCheckReferrer referrer) = 0;

  // Called from the dialog controller when the dialog is hidden.
  virtual void OnDialogHidden() = 0;

  // Called from the UI bubble controllers when OS re-auth is needed to enable
  // feature. Runs callback with true parameter immediately if user
  // authentication is not available for the given platform. Otherwise, the
  // method schedules a task to show an authentication dialog.
  // `message`is the messages to be shown in the authentication dialog after the
  // prefix "Chromium is trying to".
  virtual void AuthenticateUserWithMessage(const std::u16string& message,
                                           AvailabilityCallback callback) = 0;

  // Called from the Save/Update bubble controller when gaia re-auth is needed
  // to save passwords. This method triggers the reauth flow. Upon successful
  // reauth, it saves the password if it's still relevant. Otherwise, it changes
  // the default destination to local and reopens the save bubble.
  virtual void AuthenticateUserForAccountStoreOptInAndSavePassword(
      const std::u16string& username,
      const std::u16string& password) = 0;

  // Called from the Save/Update bubble controller when a "new" user (i.e. who
  // hasn't chosen whether to use the account-scoped storage yet) saves a
  // password (locally). If the reauth is successful, this moves the just-saved
  // password into the account store.
  virtual void
  AuthenticateUserForAccountStoreOptInAfterSavingLocallyAndMovePassword() = 0;

  // Called from Biometric Authentication promo dialog when the feature is
  // enabled.
  virtual void ShowBiometricActivationConfirmation() = 0;

  // Called from the Management bubble when user wants to save local password in
  // the account. It opens the Move bubble for the selected password.
  virtual void ShowMovePasswordBubble(
      const password_manager::PasswordForm& form) = 0;

  // Called when user clicked "No thanks" button on Biometric Authentication
  // before filling promo dialog.
  virtual void OnBiometricAuthBeforeFillingDeclined() = 0;

  // Called when user clicked "Add username" button in AddUsername bubble.
  virtual void OnAddUsernameSaveClicked(
      const std::u16string& username,
      const password_manager::PasswordForm& password_to_change) = 0;

  // Called from the Save/Update bubble controller to decide whether or not we
  // should show the user the Chrome for iOS promo.
  virtual void MaybeShowIOSPasswordPromo() = 0;

  // Called from the Relaunch Chrome bubble to gracefully restart the Chrome.
  virtual void RelaunchChrome() = 0;

 protected:
  virtual ~PasswordsModelDelegate() = default;
};

base::WeakPtr<PasswordsModelDelegate> PasswordsModelDelegateFromWebContents(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_MODEL_DELEGATE_H_
