// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/image/image.h"

namespace favicon_base {
struct FaviconImageResult;
}

namespace gfx {
class RectF;
}

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;
class PasswordManualFallbackMetricsRecorder;
class PasswordSuggestionGenerator;

// This class is responsible for filling password forms.
class PasswordAutofillManager : public autofill::AutofillSuggestionDelegate {
 public:
  PasswordAutofillManager(PasswordManagerDriver* password_manager_driver,
                          autofill::AutofillClient* autofill_client,
                          PasswordManagerClient* password_client);

  PasswordAutofillManager(const PasswordAutofillManager&) = delete;
  PasswordAutofillManager& operator=(const PasswordAutofillManager&) = delete;

  ~PasswordAutofillManager() override;

  // AutofillSuggestionDelegate implementation.
  absl::variant<autofill::AutofillDriver*, PasswordManagerDriver*> GetDriver()
      override;
  void OnSuggestionsShown(
      base::span<const autofill::Suggestion> suggestions) override;
  void OnSuggestionsHidden() override;
  void DidSelectSuggestion(const autofill::Suggestion& suggestion) override;
  void DidAcceptSuggestion(const autofill::Suggestion& suggestion,
                           const SuggestionMetadata& metadata) override;
  void DidPerformButtonActionForSuggestion(
      const autofill::Suggestion&,
      const autofill::SuggestionButtonAction&) override;
  bool RemoveSuggestion(const autofill::Suggestion& suggestion) override;
  void ClearPreviewedForm() override;
  autofill::FillingProduct GetMainFillingProduct() const override;

  // Invoked when a password mapping is added.
  void OnAddPasswordFillData(const autofill::PasswordFormFillData& fill_data);

  // Removes the credentials previously saved via OnAddPasswordFormMapping.
  void DeleteFillData();

  // Handles a request from the renderer to show a popup with the suggestions
  // from the password manager.
  void OnShowPasswordSuggestions(
      autofill::FieldRendererId element_id,
      autofill::AutofillSuggestionTriggerSource trigger_source,
      base::i18n::TextDirection text_direction,
      const std::u16string& typed_username,
      ShowWebAuthnCredentials show_webauthn_credentials,
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

  PasswordManualFallbackMetricsRecorder&
  GetPasswordManualFallbackMetricsRecorder() {
    return CHECK_DEREF(manual_fallback_metrics_recorder_.get());
  }

  // A public version of FillSuggestion(), only for use in tests.
  bool FillSuggestionForTest(const std::u16string& username);

  // A public version of PreviewSuggestion(), only for use in tests.
  bool PreviewSuggestionForTest(const std::u16string& username);

  void SetManualFallbackFlowForTest(
      std::unique_ptr<PasswordSuggestionFlow> manual_fallback_flow);

  inline PasswordSuggestionFlow* manual_fallback_flow() {
    return manual_fallback_flow_.get();
  }

  base::WeakPtr<PasswordAutofillManager> GetWeakPtr();

 private:
  // Validates and forwards the given objects to the autofill client.
  bool ShowPopup(const gfx::RectF& bounds,
                 base::i18n::TextDirection text_direction,
                 const std::vector<autofill::Suggestion>& suggestions,
                 bool is_for_webauthn_request);

  // Validates and forwards the given objects to the autofill client.
  void UpdatePopup(std::vector<autofill::Suggestion> suggestions);

  // Attempts to find and fill the suggestions with the user name |username| and
  // the `type` indicating the store (account-stored or local).
  // Returns true if it was successful.
  bool FillSuggestion(const std::u16string& username,
                      autofill::SuggestionType type);

  // Attempts to find and preview the suggestions with the user name |username|
  // and the `type` indicating the store (account-stored or
  // local). Returns true if it was successful.
  bool PreviewSuggestion(const std::u16string& username,
                         autofill::SuggestionType type);

  // If one of the login mappings in |fill_data| matches |current_username| and
  // `type` (indicating whether a credential is stored in account
  // or locally), return true and assign the password and the original signon
  // realm to |password_and_meta_data|. Note that if the credential comes from
  // the same realm as the one we're filling to, the |realm| field will be left
  // empty, as this is the behavior of |PasswordFormFillData|.
  // Otherwise, returns false and leaves |password_and_meta_data| untouched.
  bool GetPasswordAndMetadataForUsername(
      const std::u16string& current_username,
      autofill::SuggestionType type,
      const autofill::PasswordFormFillData& fill_data,
      autofill::PasswordAndMetadata* password_and_meta_data);

  // Makes a request to the favicon service for the icon of |url|.
  void RequestFavicon(const GURL& url);

  // Called when the favicon was retrieved. When the icon is not ready or
  // unavailable a fallback globe icon is used. The request to the favicon
  // store is canceled on navigation.
  void OnFaviconReady(const favicon_base::FaviconImageResult& result);

  // Replaces `type` with a loading symbol and triggers a reauth flow to opt in
  // for the account-scoped password storage, with OnUnlockReauthCompleted as
  // callback.
  void OnUnlockItemAccepted(autofill::SuggestionType type);

  // If reauth failed, resets the suggestions to show the `type` again.
  // Otherwise, triggers either generation or filling based on the `type` that
  // was clicked.
  void OnUnlockReauthCompleted(
      autofill::SuggestionType type,
      PasswordManagerClient::ReauthSucceeded reauth_succeeded);

  // Called when the biometric reauth that guards password filling completes.
  // `type` identifies the suggestion that was selected for
  // filling.
  void OnBiometricReauthCompleted(const std::u16string& username_value,
                                  autofill::SuggestionType type,
                                  bool auth_succeded);

  // Cancels an ongoing biometric re-authentication. Usually, because
  // the filling scope has changed or because |this| is being destroyed.
  void CancelBiometricReauthIfOngoing();

  // Hides the popup.
  void HidePopup();

  std::unique_ptr<autofill::PasswordFormFillData> fill_data_;

  password_manager::PasswordSuggestionGenerator suggestion_generator_;

  // Contains the favicon for the credentials offered on the current page.
  gfx::Image page_favicon_;

  // The driver that owns |this|.
  const raw_ptr<PasswordManagerDriver> password_manager_driver_;

  const raw_ptr<autofill::AutofillClient> autofill_client_;

  const raw_ptr<PasswordManagerClient> password_client_;

  // The arguments of the last ShowPopup() call and UpdatePopup(), to be re-used
  // by OnUnlockReauthCompleted().
  autofill::AutofillClient::PopupOpenArgs last_popup_open_args_;

  // Used to track a requested favicon.
  base::CancelableTaskTracker favicon_tracker_;

  // Used to trigger a reauthentication prompt based on biometrics that needs
  // to be cleared before the password is filled. Currently only used
  // on Android, Mac and Windows.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;

  // Initialized when the user triggers the password manual fallback. This flow
  // reads all user passwords upon initialization. Hence it's reset upon main
  // frame navigation or if this `PasswordAutofillManager` is destroyed.
  std::unique_ptr<PasswordSuggestionFlow> manual_fallback_flow_;

  // Used to collect metrics around the manual fallback for password. Some of
  // the metrics are meant to be emitted only on navigation.
  // `PasswordManualFallbackMetricsRecorder` emits these metrics in its
  // destructor. Therefore, this object is destroyed and re-created on
  // navigation.
  // `AutofillContextMenuManager` accesses this member before suggestions are
  // shown. Therefore, this object is instantiated before
  // `manual_fallback_flow_` and dies when `manual_fallback_flow_` dies.
  std::unique_ptr<PasswordManualFallbackMetricsRecorder>
      manual_fallback_metrics_recorder_;

  base::WeakPtrFactory<PasswordAutofillManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_AUTOFILL_MANAGER_H_
