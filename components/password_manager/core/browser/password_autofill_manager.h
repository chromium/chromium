// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "ui/gfx/image/image.h"

namespace autofill {
class AutofillDriver;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace gfx {
class RectF;
}

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

// This class is responsible for filling password forms.
class PasswordAutofillManager : public autofill::AutofillPopupDelegate {
 public:
  PasswordAutofillManager(PasswordManagerDriver* password_manager_driver,
                          autofill::AutofillClient* autofill_client,
                          PasswordManagerClient* password_client);

  PasswordAutofillManager(const PasswordAutofillManager&) = delete;
  PasswordAutofillManager& operator=(const PasswordAutofillManager&) = delete;

  ~PasswordAutofillManager() override;

  // AutofillPopupDelegate implementation.
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void OnPopupSuppressed() override;
  void DidSelectSuggestion(
      const std::u16string& value,
      int frontend_id,
      const autofill::Suggestion::BackendId& backend_id) override;
  void DidAcceptSuggestion(const autofill::Suggestion& suggestion,
                           int position) override;
  bool GetDeletionConfirmationText(const std::u16string& value,
                                   int frontend_id,
                                   std::u16string* title,
                                   std::u16string* body) override;
  bool RemoveSuggestion(const std::u16string& value, int frontend_id) override;
  void ClearPreviewedForm() override;
  autofill::PopupType GetPopupType() const override;
  absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*> GetDriver()
      override;
  int32_t GetWebContentsPopupControllerAxId() const override;
  void RegisterDeletionCallback(base::OnceClosure deletion_callback) override;

  // Invoked when a password mapping is added.
  void OnAddPasswordFillData(const autofill::PasswordFormFillData& fill_data);

  // Removes the credentials previously saved via OnAddPasswordFormMapping.
  void DeleteFillData();

  // Handles a request from the renderer to show a popup with the suggestions
  // from the password manager. |options| should be a bitwise mask of
  // autofill::ShowPasswordSuggestionsOptions values.
  void OnShowPasswordSuggestions(base::i18n::TextDirection text_direction,
                                 const std::u16string& typed_username,
                                 int options,
                                 const gfx::RectF& bounds);

  // If there are relevant credentials for the current frame show them and
  // return true. Otherwise, return false.
  // This is currently used for cases in which the automatic generation
  // option is offered through a different UI surface than the popup
  // (e.g. via the keyboard accessory on Android).
  bool MaybeShowPasswordSuggestions(const gfx::RectF& bounds,
                                    base::i18n::TextDirection text_direction);

  // If there are relevant credentials for the current frame, shows them with
  // an additional 'generation' option and returns true. Otherwise, does nothing
  // and returns false.
  bool MaybeShowPasswordSuggestionsWithGeneration(
      const gfx::RectF& bounds,
      base::i18n::TextDirection text_direction,
      bool show_password_suggestions);

  // Called when main frame navigates. Not called for in-page navigations.
  void DidNavigateMainFrame();

  // Called if no suggestions were found. Assumed to be mutually exclusive with
  // |OnAddPasswordFillData|.
  void OnNoCredentialsFound();

  // A public version of FillSuggestion(), only for use in tests.
  bool FillSuggestionForTest(const std::u16string& username);

  // A public version of PreviewSuggestion(), only for use in tests.
  bool PreviewSuggestionForTest(const std::u16string& username);

#if defined(UNIT_TEST)
  void set_autofill_client(autofill::AutofillClient* autofill_client) {
    autofill_client_ = autofill_client;
  }
#endif  // defined(UNIT_TEST)

 private:
  using ForPasswordField = base::StrongAlias<class ForPasswordFieldTag, bool>;
  using OffersGeneration = base::StrongAlias<class OffersGenerationTag, bool>;
  using ShowAllPasswords = base::StrongAlias<class ShowAllPasswordsTag, bool>;
  using ShowPasswordSuggestions =
      base::StrongAlias<class ShowPasswordSuggestionsTag, bool>;
  using ShowWebAuthnCredentials =
      base::StrongAlias<class ShowWebAuthnCredentialsTag, bool>;

  // Builds the suggestions used to show or update the autofill popup.
  std::vector<autofill::Suggestion> BuildSuggestions(
      const std::u16string& username_filter,
      ForPasswordField for_password_field,
      ShowAllPasswords show_all_passwords,
      OffersGeneration for_generation,
      ShowPasswordSuggestions show_password_suggestions,
      ShowWebAuthnCredentials show_webauthn_credentials);

  // Called just before showing a popup to log which |suggestions| were shown.
  void LogMetricsForSuggestions(
      const std::vector<autofill::Suggestion>& suggestions) const;

  // Validates and forwards the given objects to the autofill client.
  bool ShowPopup(const gfx::RectF& bounds,
                 base::i18n::TextDirection text_direction,
                 const std::vector<autofill::Suggestion>& suggestions);

  // Validates and forwards the given objects to the autofill client.
  void UpdatePopup(const std::vector<autofill::Suggestion>& suggestions);

  // Attempts to find and fill the suggestions with the user name |username| and
  // the |item_id| indicating the store (account-stored or local). Returns true
  // if it was successful.
  bool FillSuggestion(const std::u16string& username, int item_id);

  // Attempts to find and preview the suggestions with the user name |username|
  // and the |item_id| indicating the store (account-stored or local). Returns
  // true if it was successful.
  bool PreviewSuggestion(const std::u16string& username, int item_id);

  // If one of the login mappings in |fill_data| matches |current_username| and
  // |item_id| (indicating whether a credential is stored in account or
  // locally), return true and assign the password and the original signon
  // realm to |password_and_meta_data|. Note that if the credential comes from
  // the same realm as the one we're filling to, the |realm| field will be left
  // empty, as this is the behavior of |PasswordFormFillData|.
  // Otherwise, returns false and leaves |password_and_meta_data| untouched.
  bool GetPasswordAndMetadataForUsername(
      const std::u16string& current_username,
      int item_id,
      const autofill::PasswordFormFillData& fill_data,
      autofill::PasswordAndMetadata* password_and_meta_data);

  // Makes a request to the favicon service for the icon of |url|.
  void RequestFavicon(const GURL& url);

  // Called when the favicon was retrieved. When the icon is not ready or
  // unavailable a fallback globe icon is used. The request to the favicon
  // store is canceled on navigation.
  void OnFaviconReady(const favicon_base::FaviconImageResult& result);

  // Replaces |unlock_item| with a loading symbol and triggers a reauth flow to
  // opt in for the account-scoped password storage, with
  // OnUnlockReauthCompleted as callback.
  void OnUnlockItemAccepted(autofill::PopupItemId unlock_item);

  // If reauth failed, resets the suggestions to show the |unlock_item| again.
  // Otherwise, triggers either generation or filling based on the |unlock_item|
  // that was clicked.
  void OnUnlockReauthCompleted(
      autofill::PopupItemId unlock_item,
      autofill::AutofillClient::PopupOpenArgs reopen_args,
      PasswordManagerClient::ReauthSucceeded reauth_succeeded);

  // Called when the biometric reauth that guards password filling completes.
  // |frontend_id| identifies the suggestion that was selected for filling.
  void OnBiometricReauthCompleted(const std::u16string& username_value,
                                  int frontend_id,
                                  bool auth_succeded);

  // Cancels an ongoing biometric re-authentication. Usually, because
  // the filling scope has changed or because |this| is being destroyed.
  void CancelBiometricReauthIfOngoing();

  std::unique_ptr<autofill::PasswordFormFillData> fill_data_;

  // Contains the favicon for the credentials offered on the current page.
  gfx::Image page_favicon_;

  // The driver that owns |this|.
  raw_ptr<PasswordManagerDriver> password_manager_driver_;

  raw_ptr<autofill::AutofillClient> autofill_client_;  // weak

  raw_ptr<PasswordManagerClient> password_client_;

  // If not null then it will be called in destructor.
  base::OnceClosure deletion_callback_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  // Used to trigger a reauthentication prompt based on biometrics that needs
  // to be cleared before the password is filled. Currently only used
  // on Android, Mac and Windows.
  scoped_refptr<device_reauth::BiometricAuthenticator> authenticator_;

  base::WeakPtrFactory<PasswordAutofillManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
