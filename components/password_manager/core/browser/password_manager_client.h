// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/util/type_safety/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_form_forward.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/safe_browsing/buildflags.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

class PrefService;

namespace autofill {
class AutofillDownloadManager;
class LogManager;
}  // namespace autofill

namespace favicon {
class FaviconService;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint;
enum class ReauthAccessPoint;
}  // namespace signin_metrics

namespace url {
class Origin;
}

class GURL;

#if defined(ON_FOCUS_PING_ENABLED)
namespace safe_browsing {
class PasswordProtectionService;
}
#endif

namespace password_manager {

class FieldInfoManager;
class PasswordFeatureManager;
class BiometricAuthenticator;
class PasswordFormManagerForUI;
class PasswordManagerDriver;
class PasswordManagerMetricsRecorder;
class HttpAuthManager;
class PasswordRequirementsService;
class PasswordStore;

enum SyncState {
  NOT_SYNCING,
  SYNCING_NORMAL_ENCRYPTION,
  SYNCING_WITH_CUSTOM_PASSPHRASE,
  // Sync is disabled but the user is signed in and opted in to passwords
  // account storage.
  ACCOUNT_PASSWORDS_ACTIVE_NORMAL_ENCRYPTION
};

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment.
class PasswordManagerClient {
 public:
  using CredentialsCallback = base::OnceCallback<void(const PasswordForm*)>;
  using ReauthSucceeded = util::StrongAlias<class ReauthSucceededTag, bool>;

  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // Is saving new data for password autofill and filling of saved data enabled
  // for the current profile and page? For example, saving is disabled in
  // Incognito mode. |url| describes the URL to save the password for. It is not
  // necessary the URL of the current page but can be a URL of a proxy or the
  // page that hosted the form.
  virtual bool IsSavingAndFillingEnabled(const GURL& url) const;

  // Checks if filling is enabled on the current page. Filling is disabled in
  // the presence of SSL errors on a page. |url| describes the URL to fill the
  // password for. It is not necessary the URL of the current page but can be a
  // URL of a proxy or subframe.
  // TODO(crbug.com/1071842): This method's name is misleading as it also
  // determines whether saving prompts should be shown.
  virtual bool IsFillingEnabled(const GURL& url) const;

  // Checks if manual filling fallback is enabled for the page that has |url|
  // address.
  virtual bool IsFillingFallbackEnabled(const GURL& url) const;

  // Informs the embedder of a password form that can be saved or updated in
  // password store if the user allows it. The embedder is not required to
  // prompt the user if it decides that this form doesn't need to be saved or
  // updated. Returns true if the prompt was indeed displayed.
  // There are 3 different cases when |update_password| == true:
  // 1.A change password form was submitted and the user has only one stored
  // credential. Then form_to_save.GetPendingCredentials() should correspond to
  // the unique element from |form_to_save.best_matches_|.
  // 2.A change password form was submitted and the user has more than one
  // stored credential. Then we shouldn't expect anything from
  // form_to_save.GetPendingCredentials() except correct origin, since we don't
  // know which credentials should be updated.
  // 3.A sign-in password form was submitted with a password different from
  // the stored one. In this case form_to_save.IsPasswordOverridden() == true
  // and form_to_save.GetPendingCredentials() should correspond to the
  // credential that was overidden.
  virtual bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool is_update) = 0;

  // Informs the embedder that the user can move the given |form_to_move| to
  // their account store.
  virtual void PromptUserToMovePasswordToAccount(
      std::unique_ptr<PasswordFormManagerForUI> form_to_move) = 0;

  // Informs the embedder that the user started typing a password and a password
  // prompt should be available on click on the omnibox icon.
  virtual void ShowManualFallbackForSaving(
      std::unique_ptr<PasswordFormManagerForUI> form_to_save,
      bool has_generated_password,
      bool is_update) = 0;

  // Informs the embedder that the user cleared the password field and the
  // fallback for password saving should be not available.
  virtual void HideManualFallbackForSaving() = 0;

  // Informs the embedder that the focus changed to a different input in the
  // same frame (e.g. tabbed from email to password field).
  virtual void FocusedInputChanged(
      PasswordManagerDriver* driver,
      autofill::mojom::FocusedFieldType focused_field_type) = 0;

  // Informs the embedder of a password forms that the user should choose from.
  // Returns true if the prompt is indeed displayed. If the prompt is not
  // displayed, returns false and does not call |callback|.
  // |callback| should be invoked with the chosen form.
  virtual bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) = 0;

  // Indicates if re-auth with the device is needed before filling passwords.
  // Currently only used by iOS.
  virtual bool RequiresReauthToFill();

  // Instructs the client to show the Touch To Fill UI.
  virtual void ShowTouchToFill(PasswordManagerDriver* driver);

  // Returns a pointer to a BiometricAuthenticator. Might be null if
  // BiometricAuthentication is not available for a given platform.
  virtual BiometricAuthenticator* GetBiometricAuthenticator();

  // Informs the embedder that the user has manually requested to generate a
  // password in the focused password field.
  virtual void GeneratePassword();

  // Informs the embedder that automatic signing in just happened. The form
  // returned to the site is |local_forms[0]|. |local_forms| contains all the
  // local credentials for the site. |origin| is a URL of the site the user was
  // auto signed in to.
  virtual void NotifyUserAutoSignin(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin) = 0;

  // Inform the embedder that automatic signin would have happened if the user
  // had been through the first-run experience to ensure their opt-in. |form|
  // contains the PasswordForm that would have been delivered.
  virtual void NotifyUserCouldBeAutoSignedIn(
      std::unique_ptr<PasswordForm> form) = 0;

  // Inform the embedder that the user signed in with a saved credential.
  // |submitted_manager| contains the form used and allows to move credentials.
  virtual void NotifySuccessfulLoginWithExistingPassword(
      std::unique_ptr<password_manager::PasswordFormManagerForUI>
          submitted_manager) = 0;

  // Inform the embedder that the site called 'store()'.
  virtual void NotifyStorePasswordCalled() = 0;

  // Update the CredentialCache used to display fetched credentials in the UI.
  // Currently only implemented on Android.
  virtual void UpdateCredentialCache(
      const url::Origin& origin,
      const std::vector<const PasswordForm*>& best_matches,
      bool is_blacklisted);

  // Called when a password is saved in an automated fashion. Embedder may
  // inform the user that this save has occurred.
  virtual void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> saved_form_manager) = 0;

  // Called when a password is autofilled. |best_matches| contains the
  // PasswordForm into which a password was filled: the client may choose to
  // save this to the PasswordStore, for example. |origin| is the origin of the
  // form into which a password was filled. |federated_matches| are the stored
  // federated matches relevant to the filled form, this argument may be null.
  // They are never filled, but might be needed in the UI, for example. Default
  // implementation is a noop.
  virtual void PasswordWasAutofilled(
      const std::vector<const PasswordForm*>& best_matches,
      const url::Origin& origin,
      const std::vector<const PasswordForm*>* federated_matches);

  // Sends username/password from |preferred_match| for filling in the http auth
  // prompt.
  virtual void AutofillHttpAuth(const PasswordForm& preferred_match,
                                const PasswordFormManagerForUI* form_manager);

  // Informs the embedder that user credentials were leaked.
  virtual void NotifyUserCredentialsWereLeaked(
      CredentialLeakType leak_type,
      CompromisedSitesCount saved_sites,
      const GURL& origin,
      const base::string16& username);

  // Requests a reauth for the primary account with |access_point| representing
  // where the reauth was triggered.
  // Triggers the |reauth_callback| with ReauthSucceeded(true) if
  // reauthentication succeeded.
  virtual void TriggerReauthForPrimaryAccount(
      signin_metrics::ReauthAccessPoint access_point,
      base::OnceCallback<void(ReauthSucceeded)> reauth_callback);

  // Redirects the user to a sign-in in a new tab. |access_point| is used for
  // metrics recording and represents where the sign-in was triggered.
  virtual void TriggerSignIn(signin_metrics::AccessPoint access_point);

  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() const = 0;

  // Returns the profile PasswordStore associated with this instance.
  virtual PasswordStore* GetProfilePasswordStore() const = 0;

  // Returns the account PasswordStore associated with this instance.
  virtual PasswordStore* GetAccountPasswordStore() const = 0;

  // Reports whether and how passwords are synced in the embedder. The default
  // implementation always returns NOT_SYNCING.
  virtual SyncState GetPasswordSyncState() const;

  // Returns true if last navigation page had HTTP error i.e 5XX or 4XX
  virtual bool WasLastNavigationHTTPError() const;

  // Returns true if a credential leak dialog was shown. Used by Autofill
  // Assistance to verify a password change intent. TODO(b/151391231): At the
  // moment, password change scripts don't need validation, but it may change.
  // If it doesn't change, remove this method and related code.
  virtual bool WasCredentialLeakDialogShown() const;

  // Obtains the cert status for the main frame.
  virtual net::CertStatus GetMainFrameCertStatus() const;

  // Shows the dialog where the user can accept or decline the global autosignin
  // setting as a first run experience.
  virtual void PromptUserToEnableAutosignin();

  // If this browsing session should not be persisted.
  virtual bool IsIncognito() const;

  // Returns the PasswordManager associated with this client. The non-const
  // version calls the const one.
  PasswordManager* GetPasswordManager();
  virtual const PasswordManager* GetPasswordManager() const;

  // Returns the PasswordFeatureManager associated with this client. The
  // non-const version calls the const one.
  PasswordFeatureManager* GetPasswordFeatureManager();
  virtual const PasswordFeatureManager* GetPasswordFeatureManager() const;

  // Returns the HttpAuthManager associated with this client.
  virtual HttpAuthManager* GetHttpAuthManager();

  // Returns the AutofillDownloadManager for votes uploading.
  virtual autofill::AutofillDownloadManager* GetAutofillDownloadManager();

  // Returns true if the main frame URL has a secure origin.
  virtual bool IsCommittedMainFrameSecure() const;

  // Returns the committed main frame URL.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns last committed origin of the main frame.
  virtual url::Origin GetLastCommittedOrigin() const = 0;

  // Use this to filter credentials before handling them in password manager.
  virtual const CredentialsFilter* GetStoreResultFilter() const = 0;

  // Returns a LogManager instance.
  virtual const autofill::LogManager* GetLogManager() const;

  // Record that we saw a password field on this page.
  virtual void AnnotateNavigationEntry(bool has_password_field);

  // Returns the current best guess as to the page's display language.
  virtual std::string GetPageLanguage() const;

#if defined(ON_FOCUS_PING_ENABLED) || defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // Return the PasswordProtectionService associated with this instance.
  virtual safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const = 0;
#endif

#if defined(ON_FOCUS_PING_ENABLED)
  // Checks the safe browsing reputation of the webpage when the
  // user focuses on a username/password field. This is used for reporting
  // only, and won't trigger a warning.
  virtual void CheckSafeBrowsingReputation(const GURL& form_action,
                                           const GURL& frame_url) = 0;
#endif

#if defined(PASSWORD_REUSE_DETECTION_ENABLED)
  // Checks the safe browsing reputation of the webpage where password reuse
  // happens. This is called by the PasswordReuseDetectionManager when a
  // protected password is typed on the wrong domain. This may trigger a
  // warning dialog if it looks like the page is phishy.
  // The |username| is the user name of the reused password. The user name
  // can be an email or a username for a non-GAIA or saved-password reuse. No
  // validation has been done on it.
  virtual void CheckProtectedPasswordEntry(
      metrics_util::PasswordType reused_password_type,
      const std::string& username,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      bool password_field_exists) = 0;
#endif

#if defined(PASSWORD_REUSE_WARNING_ENABLED)
  // Records a Chrome Sync event that GAIA password reuse was detected.
  virtual void LogPasswordReuseDetectedEvent() = 0;
#endif

  // Gets a ukm::SourceId that is associated with the WebContents object
  // and its last committed main frame navigation.
  virtual ukm::SourceId GetUkmSourceId() = 0;

  // Gets a metrics recorder for the currently committed navigation.
  // As PasswordManagerMetricsRecorder submits metrics on destruction, a new
  // instance will be returned for each committed navigation. A caller must not
  // hold on to the pointer. This method returns a nullptr if the client
  // does not support metrics recording.
  virtual PasswordManagerMetricsRecorder* GetMetricsRecorder() = 0;

  // Gets the PasswordRequirementsService associated with the client. It is
  // valid that this method returns a nullptr if the PasswordRequirementsService
  // has not been implemented for a specific platform or the context is an
  // incognito context. Callers should guard against this.
  virtual PasswordRequirementsService* GetPasswordRequirementsService();

  // Returns the favicon service used to retrieve icons for an origin.
  virtual favicon::FaviconService* GetFaviconService();

  // Returns the identity manager for profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns a pointer to the URLLoaderFactory owned by the storage partition of
  // the current profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns a pointer to the NetworkContext owned by the storage partition of
  // the current profile.
  virtual network::mojom::NetworkContext* GetNetworkContext() const;

  // Whether the primary account of the current profile is under Advanced
  // Protection - a type of Google Account that helps protect our most at-risk
  // users.
  virtual bool IsUnderAdvancedProtection() const;

  // Causes all live PasswordFormManager objects to query the password store
  // again. Results in updating the fill information on the page.
  virtual void UpdateFormManagers() {}

  // Causes a navigation to the manage passwords page.
  virtual void NavigateToManagePasswordsPage(ManagePasswordsReferrer referrer) {
  }

  virtual bool IsIsolationForPasswordSitesEnabled() const = 0;

  // Returns true if the current page is to the new tab page.
  virtual bool IsNewTabPage() const = 0;

  // Returns a FieldInfoManager associated with the current profile.
  virtual FieldInfoManager* GetFieldInfoManager() const = 0;

  // Returns the currently set autofill-assistant mode.
  virtual AutofillAssistantMode GetAutofillAssistantMode() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
