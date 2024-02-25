// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

namespace autofill {
class AutofillClient;
}  // namespace autofill

namespace password_manager {

class PasswordManagerDriver;
class PasswordManagerClient;

// Displays all available passwords password suggestions on password and
// non-password forms for all available passwords.
class PasswordManualFallbackFlow : public autofill::AutofillPopupDelegate,
                                   public PasswordSuggestionFlow,
                                   public SavedPasswordsPresenter::Observer {
 public:
  PasswordManualFallbackFlow(
      PasswordManagerDriver* password_manager_driver,
      autofill::AutofillClient* autofill_client,
      PasswordManagerClient* password_client,
      std::unique_ptr<SavedPasswordsPresenter> passwords_presenter);
  PasswordManualFallbackFlow(const PasswordManualFallbackFlow&) = delete;
  PasswordManualFallbackFlow& operator=(const PasswordManualFallbackFlow&) =
      delete;
  ~PasswordManualFallbackFlow() override;

  static bool SupportsSuggestionType(autofill::PopupItemId popup_item_id);

  // Generates suggestions and shows the Autofill popup if the passwords were
  // already read from disk. Otherwise, saves the input parameters to run the
  // flow when the passwords are read from disk.
  void RunFlow(const gfx::RectF& bounds,
               base::i18n::TextDirection text_direction) override;

  // AutofillPopupDelegate:
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void DidSelectSuggestion(const autofill::Suggestion& suggestion) override;
  void DidAcceptSuggestion(const autofill::Suggestion& suggestion,
                           const SuggestionPosition& position) override;
  void DidPerformButtonActionForSuggestion(
      const autofill::Suggestion& suggestion) override;
  bool RemoveSuggestion(const autofill::Suggestion& suggestion) override;
  void ClearPreviewedForm() override;
  autofill::FillingProduct GetMainFillingProduct() const override;
  int32_t GetWebContentsPopupControllerAxId() const override;
  void RegisterDeletionCallback(base::OnceClosure deletion_callback) override;

 private:
  // Is used to track whether the flow was invoked and whether the passwords
  // were retrieved from disk.
  enum class FlowState {
    // The flow instance was created, but not invoked. The passwords are not
    // read from disk.
    kCreated,
    // The flow was invoked, but the passwords were not read from disk yet.
    kInvokedWithoutPasswords,
    // The passwords were read from disk. The flow might or might not have been
    // invoked already.
    kPasswordsRetrived,
  };
  // SavedPasswordsPresenter::Observer:
  void OnSavedPasswordsChanged(const PasswordStoreChangeList& changes) override;
  // Generates manual fallback suggestions and opens the Autofill popup. This
  // function assumes that passwords have been read from disk.
  void RunFlowImpl(const gfx::RectF& bounds,
                   base::i18n::TextDirection text_direction);

  const PasswordSuggestionGenerator suggestion_generator_;
  const raw_ptr<PasswordManagerDriver> password_manager_driver_;
  const raw_ptr<autofill::AutofillClient> autofill_client_;
  const raw_ptr<PasswordManagerClient> password_client_;

  // Flow state changes the following way:
  //
  // * it is initialized with `kCreated` when the flow is created.
  // * if `Run()` is called before the passwords are read from disk, it is
  // changed to `kInvokedWithoutPasswords`.
  // * it is changed to `kPasswordsAvailable` when the passwords are read from
  // disk by the `SavedPasswordsPresenter`.
  FlowState flow_state_ = FlowState::kCreated;
  std::optional<gfx::RectF> saved_bounds_;
  std::optional<base::i18n::TextDirection> saved_text_direction_;
  // Reads passwords from disk and
  std::unique_ptr<SavedPasswordsPresenter> passwords_presenter_;
  base::ScopedObservation<SavedPasswordsPresenter,
                          SavedPasswordsPresenter::Observer>
      passwords_presenter_observation_{this};

  // If not null then it will be called in destructor.
  base::OnceClosure deletion_callback_;

  base::WeakPtrFactory<PasswordManualFallbackFlow> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
