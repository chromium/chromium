// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/hsts_query.h"
#include "components/password_manager/core/browser/http_auth_manager.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/webauthn_credentials_delegate.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sync/service/sync_service.h"
#include "net/cert/cert_status_flags.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
#include "base/i18n/rtl.h"
#include "components/password_manager/core/browser/password_cross_domain_confirmation_popup_controller.h"
#include "ui/gfx/geometry/rect_f.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/first_cct_page_load_passwords_ukm_recorder.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefService;

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace autofill {
class AutofillCrowdsourcingManager;
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

namespace safe_browsing {
class PasswordProtectionService;
}

namespace device_reauth {
class DeviceAuthenticator;
}

namespace version_info {
enum class Channel;
}

namespace webauthn {
#if BUILDFLAG(IS_ANDROID)
class WebAuthnCredManDelegate;
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace webauthn

namespace password_manager {

class FieldInfoManager;
#if BUILDFLAG(IS_ANDROID)
class FirstCctPageLoadPasswordsUkmRecorder;
#endif  // BUILDFLAG(IS_ANDROID)
class PasswordFeatureManager;
class PasswordFormManagerForUI;
class PasswordManagerDriver;
class PasswordManagerInterface;
class PasswordManagerMetricsRecorder;
class HttpAuthManager;
class PasswordRequirementsService;
class PasswordReuseManager;
class PasswordStoreInterface;
class WebAuthnCredentialsDelegate;
struct PasswordForm;

enum class ErrorMessageFlowType { kSaveFlow, kFillFlow };

#if BUILDFLAG(IS_ANDROID)
struct PasswordFillingParams {
  autofill::FormData form;
  uint64_t username_field_index;
  uint64_t password_field_index;
  autofill::FieldRendererId focused_field_renderer_id_;
  // TODO(crbug.com/40274966): Remove this param after
  // PasswordSuggestionBottomSheetV2 is launched.
  autofill::mojom::SubmissionReadinessState submission_readiness;
};
#endif  // BUILDFLAG(IS_ANDROID)

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment. PasswordManagerClient is instantiated once per WebContents.
// Main frame w.r.t WebContents refers to the primary main frame so usages of
// main frame here are also referring to the primary main frame.
class PasswordManagerClient {
 public:
  using CredentialsCallback = base::OnceCallback<void(const PasswordForm*)>;
  using ReauthSucceeded = base::StrongAlias<class ReauthSucceededTag, bool>;

  PasswordManagerClient() = default;

  PasswordManagerClient(const PasswordManagerClient&) = delete;
  PasswordManagerClient& operator=(const PasswordManagerClient&) = delete;

  virtual ~PasswordManagerClient() = default;

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
  // TODO(crbug.com/40685327): This method's name is misleading as it also
  // determines whether saving prompts should be shown.
  virtual bool IsFillingEnabled(const GURL& url) const;

  // Checks if the auto sign-in functionality is enabled.
  virtual bool IsAutoSignInEnabled() const;

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
      autofill::FieldRendererId focused_field_id,
      autofill::mojom::FocusedFieldType focused_field_type) = 0;

  // Informs the embedder of a password forms that the user should choose from.
  // Returns true if the prompt is indeed displayed. If the prompt is not
  // displayed, returns false and does not call |callback|.
  // |callback| should be invoked with the chosen form.
  virtual bool PromptUserToChooseCredentials(
      std::vector<std::unique_ptr<PasswordForm>> local_forms,
      const url::Origin& origin,
      CredentialsCallback callback) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Shows the error message that suggests the user to sign in to "save" or
  // "use" passwords, depending on the |flow_type|. If the |error_type|
  // indicates that signing in again won't help, the message won't be shown.
  virtual void ShowPasswordManagerErrorMessage(
      ErrorMessageFlowType flow_type,
      password_manager::PasswordStoreBackendErrorType error_type);

  // Instructs the client to show a keyboard replacing surface UI (e.g.
  // TouchToFill). `shown_cb` will be invoked with whether the view was shown.
  // TODO(crbug.com/341322405): Make this synchronous again once the account
  // storage notice is gone.
  virtual void ShowKeyboardReplacingSurface(
      PasswordManagerDriver* driver,
      const PasswordFillingParams& password_filling_params,
      bool is_webauthn_form,
      base::OnceCallback<void(bool)> shown_cb);
#endif

  // Checks whether user re-authentication should be triggered before password
  // filling.
  virtual bool IsReauthBeforeFillingRequired(
      device_reauth::DeviceAuthenticator* authenticator);

  // Returns a pointer to a DeviceAuthenticator. Might be null if
  // BiometricAuthentication is not available for a given platform.
  virtual std::unique_ptr<device_reauth::DeviceAuthenticator>
  GetDeviceAuthenticator();

  // Informs the embedder that the user has requested to generate a
  // password in the focused password field.
  virtual void GeneratePassword(
      autofill::password_generation::PasswordGenerationType type);

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

  // Informs that a successful login has just happened.
  // TODO(crbug.com/40215916): Remove when the TimeToSuccessfulLogin metric is
  // deprecated.
  virtual void NotifyOnSuccessfulLogin(
      const std::u16string& submitted_username) {}

  // Informs that that Keychain is not available.
  virtual void NotifyKeychainError() = 0;

  // Informs that a credential filled by Touch To Fill can be submitted.
  // TODO(crbug.com/40215916): Remove when the TimeToSuccessfulLogin metric is
  // deprecated.
  virtual void StartSubmissionTrackingAfterTouchToFill(
      const std::u16string& filled_username) {}

  // Informs that a successful submission didn't happen after Touch To Fill
  // (e.g. a submission failed, a user edited an input field manually).
  // TODO(crbug.com/40215916): Remove when the TimeToSuccessfulLogin metric is
  // deprecated.
  virtual void ResetSubmissionTrackingAfterTouchToFill() {}

  // Inform the embedder that the site called 'store()'.
  virtual void NotifyStorePasswordCalled() = 0;

  // Update the CredentialCache used to display fetched credentials in the UI.
  // Currently only implemented on Android.
  virtual void UpdateCredentialCache(
      const url::Origin& origin,
      base::span<const PasswordForm> best_matches,
      bool is_blocklisted);

  // Called when a password is saved in an automated fashion. Embedder may
  // inform the user that this save has occurred.
  virtual void AutomaticPasswordSave(
      std::unique_ptr<PasswordFormManagerForUI> saved_form_manager,
      bool is_update_confirmation) = 0;

  // Called when a password is autofilled. |best_matches| contains the
  // PasswordForm into which a password was filled: the client may choose to
  // save this to the PasswordStore, for example. |origin| is the origin of the
  // form into which a password was filled. |federated_matches| are the stored
  // federated matches relevant to the filled form, this argument may be empty.
  // They are never filled, but might be needed in the UI, for example. Default
  // implementation is a noop. |was_autofilled_on_pageload| contains information
  // if password form was autofilled on pageload.
  virtual void PasswordWasAutofilled(
      base::span<const PasswordForm> best_matches,
      const url::Origin& origin,
      base::span<const PasswordForm> federated_matches,
      bool was_autofilled_on_pageload);

  // Sends username/password from |preferred_match| for filling in the http auth
  // prompt.
  virtual void AutofillHttpAuth(const PasswordForm& preferred_match,
                                const PasswordFormManagerForUI* form_manager);

  // Informs the embedder that user credentials were leaked.
  virtual void NotifyUserCredentialsWereLeaked(CredentialLeakType leak_type,
                                               const GURL& origin,
                                               const std::u16string& username,
                                               bool in_account_store);

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

  // Gets local state prefs.
  virtual PrefService* GetLocalStatePrefs() const = 0;

  // Gets the sync service associated with this client.
  virtual const syncer::SyncService* GetSyncService() const = 0;

  // Gets the affiliation service associated with this client.
  virtual affiliations::AffiliationService* GetAffiliationService() = 0;

  // Returns the profile PasswordStore associated with this instance.
  virtual PasswordStoreInterface* GetProfilePasswordStore() const = 0;

  // Returns the account PasswordStore associated with this instance.
  virtual PasswordStoreInterface* GetAccountPasswordStore() const = 0;

  // Returns the PasswordReuseManager associated with this instance.
  virtual PasswordReuseManager* GetPasswordReuseManager() const = 0;

  // Returns true if last navigation page had HTTP error i.e 5XX or 4XX
  virtual bool WasLastNavigationHTTPError() const;

  // Obtains the cert status for the main frame.
  // The WebContents only has a primary main frame, so MainFrame here refers to
  // the primary main frame.
  virtual net::CertStatus GetMainFrameCertStatus() const;

  // Shows the dialog where the user can accept or decline the global autosignin
  // setting as a first run experience.
  virtual void PromptUserToEnableAutosignin();

  // If this browsing session should not be persisted.
  virtual bool IsOffTheRecord() const;

  // Returns the profile type of the session.
  virtual profile_metrics::BrowserProfileType GetProfileType() const;

  // Returns the PasswordManager associated with this client. The non-const
  // version calls the const one.
  PasswordManagerInterface* GetPasswordManager();
  virtual const PasswordManagerInterface* GetPasswordManager() const;

  // Returns the PasswordFeatureManager associated with this client. The
  // non-const version calls the const one.
  PasswordFeatureManager* GetPasswordFeatureManager();
  virtual const PasswordFeatureManager* GetPasswordFeatureManager() const;

  // Returns the HttpAuthManager associated with this client.
  virtual HttpAuthManager* GetHttpAuthManager();

  // Returns the AutofillCrowdsourcingManager for votes uploading.
  virtual autofill::AutofillCrowdsourcingManager*
  GetAutofillCrowdsourcingManager();

  // Returns true if the main frame URL has a secure origin.
  // The WebContents only has a primary main frame, so MainFrame here refers to
  // the primary main frame.
  virtual bool IsCommittedMainFrameSecure() const;

  // Returns the committed main frame URL.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns last committed origin of the main frame.
  virtual url::Origin GetLastCommittedOrigin() const = 0;

  // Use this to filter credentials before handling them in password manager.
  virtual const CredentialsFilter* GetStoreResultFilter() const = 0;

  // Returns a LogManager instance.
  virtual autofill::LogManager* GetLogManager();

  // Record that we saw a password field on this page.
  virtual void AnnotateNavigationEntry(bool has_password_field);

  // Returns the current best guess as to the page's display language.
  virtual autofill::LanguageCode GetPageLanguage() const;

  // Return the PasswordProtectionService associated with this instance.
  virtual safe_browsing::PasswordProtectionService*
  GetPasswordProtectionService() const = 0;

  // Maybe triggers a hats survey that measures the user's perception of
  // Autofill for passwords. When triggering happens, the survey dialog will be
  // displayed with a 5s delay. This survey should be triggered after form
  // submissions.
  // `filling_assistance` will be logged together with the responses as
  // in-product data and should be a string representation of the
  // `FillingAssistance` enum, i.e "Manually filled".
  virtual void TriggerUserPerceptionOfPasswordManagerSurvey(
      const std::string& filling_assistance);

#if defined(ON_FOCUS_PING_ENABLED)
  // Checks the safe browsing reputation of the webpage when the
  // user focuses on a username/password field. This is used for reporting
  // only, and won't trigger a warning.
  virtual void CheckSafeBrowsingReputation(const GURL& form_action,
                                           const GURL& frame_url) = 0;
#endif

  // If the feature is enabled send an event to the enterprise reporting
  // connector server indicating that the user signed in to a website.
  virtual void MaybeReportEnterpriseLoginEvent(
      const GURL& url,
      bool is_federated,
      const url::SchemeHostPort& federated_origin,
      const std::u16string& login_user_name) const {}

  // If the feature is enabled send an event to the enterprise reporting
  // connector server indicating that the user has some leaked credentials.
  // |identities| contains the (url, username) pairs for each leaked identity.
  virtual void MaybeReportEnterprisePasswordBreachEvent(
      const std::vector<std::pair<GURL, std::u16string>>& identities) const {}

  // Gets a ukm::SourceId that is associated with the WebContents object
  // and its last committed main frame navigation.
  virtual ukm::SourceId GetUkmSourceId() = 0;

  // Gets a metrics recorder for the currently committed navigation.
  // As PasswordManagerMetricsRecorder submits metrics on destruction, a new
  // instance will be returned for each committed navigation. A caller must not
  // hold on to the pointer. This method returns a nullptr if the client
  // does not support metrics recording.
  virtual PasswordManagerMetricsRecorder* GetMetricsRecorder() = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns a metrics recorder created specifically for the first CCT page
  // load. This can return nullptr if the current tab is not a CCT, or if
  // the user already navigated away from the first page.
  // It records metrics on destruction, which happens on the first navigation
  // away from the first loaded page. Callers should  not hold on to the
  // pointer.
  virtual FirstCctPageLoadPasswordsUkmRecorder*
  GetFirstCctPageLoadUkmRecorder() = 0;
#endif
  // Gets the PasswordRequirementsService associated with the client. It is
  // valid that this method returns a nullptr if the PasswordRequirementsService
  // has not been implemented for a specific platform or the context is an
  // incognito context. Callers should guard against this.
  virtual PasswordRequirementsService* GetPasswordRequirementsService();

  // Returns the favicon service used to retrieve icons for an origin.
  virtual favicon::FaviconService* GetFaviconService();

  // Returns the identity manager for profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the field info manager for profile.
  virtual password_manager::FieldInfoManager* GetFieldInfoManager() const;

  // Returns a pointer to the URLLoaderFactory owned by the storage partition of
  // the current profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns a pointer to the NetworkContext owned by the storage partition of
  // the current profile.
  virtual network::mojom::NetworkContext* GetNetworkContext() const;

  // Causes all live PasswordFormManager objects to query the password store
  // again. Results in updating the fill information on the page.
  virtual void UpdateFormManagers() {}

  // Causes a navigation to the manage passwords page.
  virtual void NavigateToManagePasswordsPage(ManagePasswordsReferrer referrer) {
  }

#if BUILDFLAG(IS_ANDROID)
  virtual void NavigateToManagePasskeysPage(ManagePasswordsReferrer referrer) {}
#endif

  virtual bool IsIsolationForPasswordSitesEnabled() const = 0;

  // Returns true if the current page is to the new tab page.
  virtual bool IsNewTabPage() const = 0;

  // Returns the WebAuthnCredentialsDelegate for the given driver, if available.
  virtual WebAuthnCredentialsDelegate* GetWebAuthnCredentialsDelegateForDriver(
      PasswordManagerDriver* driver);

#if BUILDFLAG(IS_ANDROID)
  // Returns the WebAuthnCredManDelegate for the driver.
  virtual webauthn::WebAuthnCredManDelegate*
  GetWebAuthnCredManDelegateForDriver(PasswordManagerDriver* driver);

  // Marks all credentials that have been loaded for this page and have been
  // received via the password sharing feature as notified.
  virtual void MarkSharedCredentialsAsNotified(const GURL& url);
#endif  // BUILDFLAG(IS_ANDROID)

  // Returns the Chrome channel for the installation.
  virtual version_info::Channel GetChannel() const;

  // Refreshes password manager settings stored in prefs.
  virtual void RefreshPasswordManagerSettingsIfNeeded() const;

  // Display username/password options to the user in the "ambient" sign-in
  // bubble, which can also display other credential types for sign-in.
  // If the user selects a password from the bubble, `callback` is invoked with
  // the selected `PasswordForm`.
  virtual void ShowCredentialsInAmbientBubble(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> forms,
      int credential_type_flags,
      CredentialsCallback callback);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)

  // Shows the bubble with the details of the `form`.
  virtual void OpenPasswordDetailsBubble(
      const password_manager::PasswordForm& form) = 0;

  // Creates and show the cross domain confirmation popup.
  virtual std::unique_ptr<PasswordCrossDomainConfirmationPopupController>
  ShowCrossDomainConfirmationPopup(const gfx::RectF& element_bounds,
                                   base::i18n::TextDirection text_direction,
                                   const GURL& domain,
                                   const std::u16string& password_origin,
                                   base::OnceClosure confirmation_callback) = 0;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_CLIENT_H_
