// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_

#include <list>
#include <memory>
#include <vector>

#include "base/timer/timer.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/browser/ui/passwords/passwords_leak_dialog_delegate.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/common/buildflags.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class TimeDelta;
}

namespace content {
class WebContents;
}

namespace password_manager {
enum class CredentialType;
struct InteractionsStats;
class MovePasswordToAccountStoreHelper;
class PasswordFeatureManager;
class PasswordFormManagerForUI;
class PostSaveCompromisedHelper;
}  // namespace password_manager

namespace {
constexpr int kMaxNumberOfTimesBiometricAuthForFillingPromoWillBeShown = 3;
}

class AccountChooserPrompt;
struct AccountInfo;
class AutoSigninFirstRunPrompt;
class CredentialLeakPrompt;
class ManagePasswordsIconView;
class CredentialLeakDialogController;
class CredentialManagerDialogController;
class PasswordBaseDialogController;

// Per-tab class to control the Omnibox password icon and bubble.
class ManagePasswordsUIController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsUIController>,
      public password_manager::PasswordStoreInterface::Observer,
      public PasswordsLeakDialogDelegate,
      public PasswordsModelDelegate,
      public PasswordsClientUIDelegate {
 public:
  ManagePasswordsUIController(const ManagePasswordsUIController&) = delete;
  ManagePasswordsUIController& operator=(const ManagePasswordsUIController&) =
      delete;

  ~ManagePasswordsUIController() override;

#if defined(UNIT_TEST)
  static void set_save_fallback_timeout_in_seconds(int timeout) {
    save_fallback_timeout_in_seconds_ = timeout;
  }
#endif

  // PasswordsClientUIDelegate:
  void OnPasswordSubmitted(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager)
      override;
  void OnUpdatePasswordSubmitted(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager)
      override;
  void OnShowManualFallbackForSaving(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager,
      bool has_generated_password,
      bool is_update) override;
  void OnHideManualFallbackForSaving() override;
  bool OnChooseCredentials(
      std::vector<std::unique_ptr<password_manager::PasswordForm>>
          local_credentials,
      const url::Origin& origin,
      ManagePasswordsState::CredentialsCallback callback) override;
  void OnAutoSignin(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> local_forms,
      const url::Origin& origin) override;
  void OnPromptEnableAutoSignin() override;
  void OnAutomaticPasswordSave(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_manager)
      override;
  void OnPasswordAutofilled(
      const std::vector<const password_manager::PasswordForm*>& password_forms,
      const url::Origin& origin,
      const std::vector<const password_manager::PasswordForm*>*
          federated_matches) override;
  void OnCredentialLeak(password_manager::CredentialLeakType leak_dialog_type,
                        const GURL& url,
                        const std::u16string& username) override;
  void OnShowMoveToAccountBubble(
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_move)
      override;
  void OnBiometricAuthenticationForFilling(PrefService* prefs) override;
  void ShowBiometricActivationConfirmation() override;
  void OnBiometricAuthBeforeFillingDeclined() override;

  virtual void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<password_manager::PasswordForm> unsynced_credentials);

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // Set the state of the Omnibox icon, and possibly show the associated bubble
  // without user interaction.
  virtual void UpdateIconAndBubbleState(ManagePasswordsIconView* icon);

  // True iff the bubble is to be opened automatically.
  bool IsAutomaticallyOpeningBubble() const {
    return bubble_status_ == BubbleStatus::SHOULD_POP_UP;
  }

  // virtual to be overridden in tests.
  virtual base::WeakPtr<PasswordsModelDelegate> GetModelDelegateProxy();

  // PasswordsModelDelegate:
  content::WebContents* GetWebContents() const override;
  url::Origin GetOrigin() const override;
  password_manager::PasswordFormMetricsRecorder*
  GetPasswordFormMetricsRecorder() override;
  password_manager::PasswordFeatureManager* GetPasswordFeatureManager()
      override;
  password_manager::ui::State GetState() const override;
  const password_manager::PasswordForm& GetPendingPassword() const override;
  const std::vector<password_manager::PasswordForm>& GetUnsyncedCredentials()
      const override;
  password_manager::metrics_util::CredentialSourceType GetCredentialSource()
      const override;
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCurrentForms() const override;
  const password_manager::InteractionsStats* GetCurrentInteractionStats()
      const override;
  size_t GetTotalNumberCompromisedPasswords() const override;
  bool DidAuthForAccountStoreOptInFail() const override;
  bool BubbleIsManualFallbackForSaving() const override;
  void OnBubbleShown() override;
  void OnBubbleHidden() override;
  void OnNoInteraction() override;
  void OnNopeUpdateClicked() override;
  void NeverSavePassword() override;
  void OnPasswordsRevealed() override;
  void SavePassword(const std::u16string& username,
                    const std::u16string& password) override;
  void SaveUnsyncedCredentialsInProfileStore(
      const std::vector<password_manager::PasswordForm>& selected_credentials)
      override;
  void DiscardUnsyncedCredentials() override;
  void MovePasswordToAccountStore() override;
  void BlockMovingPasswordToAccountStore() override;
  void ChooseCredential(
      const password_manager::PasswordForm& form,
      password_manager::CredentialType credential_type) override;
  void NavigateToPasswordManagerAccountDashboard(
      password_manager::ManagePasswordsReferrer referrer) override;
  void NavigateToPasswordManagerSettingsPage(
      password_manager::ManagePasswordsReferrer referrer) override;
  void EnableSync(const AccountInfo& account) override;
  void OnDialogHidden() override;
  // TODO(crbug.com/1353344): Replace AuthenticateUser with
  // AuthenticateUserWithMessage
  bool AuthenticateUser() override;
  void AuthenticateUserWithMessage(const std::u16string& message,
                                   AvailabilityCallback callback) override;
  void AuthenticateUserForAccountStoreOptInAndSavePassword(
      const std::u16string& username,
      const std::u16string& password) override;
  void AuthenticateUserForAccountStoreOptInAndMovePassword() override;
  void AuthenticateUserForAccountStoreOptInAfterSavingLocallyAndMovePassword()
      override;
  bool ArePasswordsRevealedWhenBubbleIsOpened() const override;

#if defined(UNIT_TEST)
  // Overwrites the client for |passwords_data_|.
  void set_client(password_manager::PasswordManagerClient* client) {
    passwords_data_.set_client(client);
  }
#endif  // defined(UNIT_TEST)

 protected:
  explicit ManagePasswordsUIController(content::WebContents* web_contents);

  // Hides the bubble if opened. Mocked in the tests.
  virtual void HidePasswordBubble();

  // Called when a PasswordForm is autofilled, when a new PasswordForm is
  // submitted, or when a navigation occurs to update the visibility of the
  // manage passwords icon and bubble.
  virtual void UpdateBubbleAndIconVisibility();

  // Called to create the account chooser dialog. Mocked in tests.
  virtual AccountChooserPrompt* CreateAccountChooser(
      CredentialManagerDialogController* controller);

  // Called to create the account chooser dialog. Mocked in tests.
  virtual AutoSigninFirstRunPrompt* CreateAutoSigninPrompt(
      CredentialManagerDialogController* controller);

  // Called to create the credentials leaked dialog.
  virtual CredentialLeakPrompt* CreateCredentialLeakPrompt(
      CredentialLeakDialogController* controller);

  // Check if |web_contents()| is attached to some Browser. Mocked in tests.
  virtual bool HasBrowserWindow() const;

  // True if the bubble is to be opened automatically or after re-auth.
  bool ShouldBubblePopUp() const {
    return IsAutomaticallyOpeningBubble() ||
           bubble_status_ == BubbleStatus::SHOULD_POP_UP_AFTER_REAUTH;
  }

  // Returns whether the bubble is currently open.
  bool IsShowingBubbleForTest() const { return IsShowingBubble(); }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class content::WebContentsUserData<ManagePasswordsUIController>;

  void OnReauthCompleted();

  // PasswordsLeakDialogDelegate:
  void NavigateToPasswordCheckup(
      password_manager::PasswordCheckReferrer referrer) override;
  void OnLeakDialogHidden() override;

  enum class BubbleStatus {
    NOT_SHOWN,
    // The bubble is to be popped up in the next call to
    // UpdateBubbleAndIconVisibility().
    SHOULD_POP_UP,
    // The bubble is to be reopened after re-authentication.
    SHOULD_POP_UP_AFTER_REAUTH,
    SHOWN,
    // Same as SHOWN but the icon is to be updated when the bubble is closed.
    SHOWN_PENDING_ICON_UPDATE,
  };

  bool IsShowingBubble() const {
    return bubble_status_ == BubbleStatus::SHOWN ||
           bubble_status_ == BubbleStatus::SHOWN_PENDING_ICON_UPDATE;
  }

  // Returns the timeout for the manual save fallback.
  static base::TimeDelta GetTimeoutForSaveFallback();

  // Shows the password bubble without user interaction.
  void ShowBubbleWithoutUserInteraction();

  // Resets |bubble_status_| signalling that if the bubble was due to pop up,
  // it shouldn't anymore.
  void ClearPopUpFlagForBubble();

  // Closes the account chooser gracefully so the callback is called. Closes the
  // password bubble. Then sets the state to MANAGE_STATE.
  void DestroyPopups();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Requests authentication and reopens the bubble if the controller still
  // exists and is in a pending state.
  void RequestAuthenticationAndReopenBubble();

  // Re-opens the bubble. The password in the reopened bubble will be revealed
  // if the authentication was successful.
  void ReopenBubbleAfterAuth(bool auth_is_successful);

  // Shows an authentication dialog and returns true if auth is successful.
  virtual bool ShowAuthenticationDialog();

  // Gets invoked gaia reauth flow is finished. If the reauth was successful,
  // and the |form_manager| is still the same, |username| and |password| are
  // saved against the current origin. If the reauth was unsuccessful, it
  // changes the default destination to profle store and reopens the save
  // bubble.
  void FinishSavingPasswordAfterAccountStoreOptInAuth(
      const url::Origin& origin,
      password_manager::PasswordFormManagerForUI* form_manager,
      const std::u16string& username,
      const std::u16string& password,
      password_manager::PasswordManagerClient::ReauthSucceeded
          reauth_succeeded);

  void OnTriggerPostSaveCompromisedBubble(
      password_manager::PostSaveCompromisedHelper::BubbleType type,
      size_t count_compromised_passwords_);

  // Triggered from a reauthentication flow. If |form_manager| is still valid
  // and the reauth was successful, the password is moved to the account store.
  void FinishMovingPasswordAfterAccountStoreOptInAuth(
      password_manager::PasswordFormManagerForUI* form_manager,
      password_manager::PasswordManagerClient::ReauthSucceeded
          reauth_succeeded);

  // Called from an opt-in/reauth flow that was triggered after a new
  // account-storage-eligible user saved a password locally. If the opt-in was
  // successful, this moves the just-saved password into the account store.
  void MoveJustSavedPasswordAfterAccountStoreOptIn(
      password_manager::PasswordForm form,
      password_manager::PasswordManagerClient::ReauthSucceeded
          reauth_succeeded);

  void OnMoveJustSavedPasswordAfterAccountStoreOptInCompleted(
      std::list<std::unique_ptr<
          password_manager::MovePasswordToAccountStoreHelper>>::iterator
          done_helper_it);

  // Cancels current authentication and releases |biometric_authenticator_|.
  void CancelAnyOngoingBiometricAuth();

  // Timeout in seconds for the manual fallback for saving.
  static int save_fallback_timeout_in_seconds_;

  // The wrapper around current state and data.
  ManagePasswordsState passwords_data_;

  // The controller for the blocking dialogs.
  std::unique_ptr<PasswordBaseDialogController> dialog_controller_;

  // The helper to pop up a reminder about compromised passwords.
  std::unique_ptr<password_manager::PostSaveCompromisedHelper>
      post_save_compromised_helper_;

  BubbleStatus bubble_status_ = BubbleStatus::NOT_SHOWN;

  // The timer that controls whether the fallback for saving should be
  // available. Should be reset once the fallback is not needed (an automatic
  // popup will be shown or the user saved/updated the password with the
  // fallback).
  base::OneShotTimer save_fallback_timer_;

  // True iff bubble should pop up with revealed password value.
  bool are_passwords_revealed_when_next_bubble_is_opened_;

  // Contains the helpers currently executing moving tasks. This will almost
  // always contain either 0 or 1 items.
  std::list<std::unique_ptr<password_manager::MovePasswordToAccountStoreHelper>>
      move_to_account_store_helpers_;

  scoped_refptr<device_reauth::BiometricAuthenticator> biometric_authenticator_;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  bool was_biometric_authentication_for_filling_promo_shown_ = false;
#endif

  // The bubbles of different types can pop up unpredictably superseding each
  // other. However, closing the bubble may affect the state of
  // ManagePasswordsUIController internally. This is undesired if
  // ManagePasswordsUIController is in the process of opening a new bubble. The
  // situation is worse on Windows where the bubble is destroyed asynchronously.
  // Thus, OnBubbleHidden() can be called after the new one is shown. By that
  // time the internal state of ManagePasswordsUIController has nothing to do
  // with the old bubble.
  // Invalidating all the weak pointers will detach the current bubble.
  base::WeakPtrFactory<ManagePasswordsUIController> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_UI_CONTROLLER_H_
