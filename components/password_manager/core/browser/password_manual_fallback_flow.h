// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/password_manager/core/browser/password_cross_domain_confirmation_popup_controller.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

namespace autofill {
class AutofillClient;
}  // namespace autofill

namespace device_reauth {
class DeviceAuthenticator;
}  // namespace device_reauth

namespace password_manager {

class FormFetcherImpl;
class PasswordFormCache;
class PasswordManagerDriver;
class PasswordManagerClient;
class PasswordManualFallbackMetricsRecorder;

// Displays all available passwords password suggestions on password and
// non-password forms for all available passwords.
class PasswordManualFallbackFlow : public autofill::AutofillSuggestionDelegate,
                                   public PasswordSuggestionFlow,
                                   public SavedPasswordsPresenter::Observer,
                                   public FormFetcher::Consumer {
 public:
  PasswordManualFallbackFlow(
      PasswordManagerDriver* password_manager_driver,
      autofill::AutofillClient* autofill_client,
      PasswordManagerClient* password_client,
      PasswordManualFallbackMetricsRecorder* manual_fallback_metrics_recorder,
      const PasswordFormCache* password_form_cache,
      std::unique_ptr<SavedPasswordsPresenter> passwords_presenter);
  PasswordManualFallbackFlow(const PasswordManualFallbackFlow&) = delete;
  PasswordManualFallbackFlow& operator=(const PasswordManualFallbackFlow&) =
      delete;
  ~PasswordManualFallbackFlow() override;

  static bool SupportsSuggestionType(autofill::SuggestionType type);

  // Generates suggestions and shows the Autofill popup if the passwords were
  // already read from disk. Otherwise, saves the input parameters to run the
  // flow when the passwords are read from disk.
  void RunFlow(autofill::FieldRendererId field_id,
               const gfx::RectF& bounds,
               base::i18n::TextDirection text_direction) override;

  // AutofillSuggestionDelegate:
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

 private:
  // Is used to track whether the flow was invoked and whether the passwords
  // were retrieved from disk.
  enum class FlowState {
    // The flow instance was created, but not invoked. The passwords are not
    // read from disk.
    kCreated,
    // The passwords for the "Suggested" passwords section have been fetched.
    // The passwords for the "All passwords" have not been fetched. Refer to
    // `PasswordSuggestionGenerator::GetManualFallbackSuggestions` for more
    // information on the password suggestion sections.
    kSuggestedPasswordsReady,
    // The passwords for the "All passwords" passwords section have been
    // fetched. The passwords for the "Suggested" have not been fetched. Refer
    // to `PasswordSuggestionGenerator::GetManualFallbackSuggestions` for more
    // information on the password suggestion sections.
    kAllUserPasswordsFetched,
    // Both relevant passwords for the current domain and all user passwords
    // have been fetched.
    kFlowInitialized,
  };
  // FormFetcher::Consumer:
  void OnFetchCompleted() override;
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(const PasswordStoreChangeList& changes) override;
  // Generates manual fallback suggestions and opens the Autofill popup. This
  // function assumes that passwords have been read from disk.
  void RunFlowImpl(const gfx::RectF& bounds,
                   base::i18n::TextDirection text_direction);
  // Authenticates the user before filling any values into the fields if the
  // authentication is configured for the device. `fill_fields` is used to fill
  // values into the fields.
  void MaybeAuthenticateBeforeFilling(base::OnceClosure fill_fields);
  // Executed when the biometric reautch that guards password filling completes.
  // `fill_fields` is used to fill values into the fields.
  void OnBiometricReauthCompleted(base::OnceClosure fill_fields,
                                  bool auth_succeeded);
  // Cancels an ongoing biometric re-authentication.
  void CancelBiometricReauthIfOngoing();

  // For credentials not originated from the current domain (defined by
  // `payload.is_cross_domain`), this method makes sure that the `on_allowed`
  // callback is called only after receiving user's explicit consent (through
  // a bubble dialog).
  // For non cross domain usages the `on_allowed` is called immediately.
  void EnsureCrossDomainPasswordUsageGetsConsent(
      const autofill::Suggestion::PasswordSuggestionDetails& payload,
      base::OnceClosure on_allowed);

  // Returns whether `field_id_` represents a username or password field in
  // `password_form_`. This only considers the username and password fields
  // stored in the `PasswordForm` object.
  bool IsTriggerFieldRelevantInPasswordForm(
      const PasswordForm* password_form) const;

  const PasswordSuggestionGenerator suggestion_generator_;
  const raw_ptr<PasswordManagerDriver> password_manager_driver_;
  const raw_ptr<autofill::AutofillClient> autofill_client_;
  const raw_ptr<PasswordManagerClient> password_client_;
  const raw_ptr<PasswordManualFallbackMetricsRecorder>
      manual_fallback_metrics_recorder_;
  const raw_ptr<const PasswordFormCache> password_form_cache_;

  // Flow state changes the following way:
  //
  // * it is initialized with `kCreated` when the flow is created.
  // * if `Run()` is called before the passwords are read from disk, it is
  // changed to `kInvokedWithoutPasswords`.
  // * it is changed to `kPasswordsAvailable` when the passwords are read from
  // disk by the `SavedPasswordsPresenter`.
  FlowState flow_state_ = FlowState::kCreated;

  // This is used to delay the flow invocation whenever the flow is run while
  // some data is not yet fetched. This member is initialized only if at least
  // one invocation is delayed.
  base::OnceClosure on_all_password_data_ready_;

  // The latest `RunFlow()` call arguments.
  autofill::FieldRendererId field_id_;
  gfx::RectF bounds_;
  base::i18n::TextDirection text_direction_;

  // Fetches user passwords relevant for the current domain.
  std::unique_ptr<FormFetcherImpl> form_fetcher_;
  // Reads all user passwords from disk.
  std::unique_ptr<SavedPasswordsPresenter> passwords_presenter_;
  base::ScopedObservation<SavedPasswordsPresenter,
                          SavedPasswordsPresenter::Observer>
      passwords_presenter_observation_{this};

  // Used to trigger a reauthentication prompt based on biometrics that needs
  // to be cleared before the password is filled. Currently only used
  // on Android, Mac and Windows.
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<PasswordCrossDomainConfirmationPopupController>
      cross_domain_confirmation_popup_controller_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<PasswordManualFallbackFlow> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
