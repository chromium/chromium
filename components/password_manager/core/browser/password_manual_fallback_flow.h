// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_

#include "base/functional/callback_forward.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/password_suggestion_generator.h"

namespace autofill {
class AutofillClient;
}  // namespace autofill

namespace password_manager {

class PasswordManagerDriver;
class PasswordManagerClient;

// Displays all available passwords password suggestions on password and
// non-password forms for all available passwords.
class PasswordManualFallbackFlow : public autofill::AutofillPopupDelegate {
 public:
  PasswordManualFallbackFlow(PasswordManagerDriver* password_manager_driver,
                             autofill::AutofillClient* autofill_client,
                             PasswordManagerClient* password_client);
  PasswordManualFallbackFlow(const PasswordManualFallbackFlow&) = delete;
  PasswordManualFallbackFlow& operator=(const PasswordManualFallbackFlow&) =
      delete;
  ~PasswordManualFallbackFlow() override;

  static bool SupportsSuggestionType(autofill::PopupItemId popup_item_id);

  // Generates suggestions and shows the Autofill popup if the passwords were
  // already read from disk. Otherwise, saves the input parameters to run the
  // flow when the passwords are read from disk.
  void RunFlow();

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
  const raw_ptr<PasswordManagerDriver> password_manager_driver_;
  const raw_ptr<autofill::AutofillClient> autofill_client_;
  const raw_ptr<PasswordManagerClient> password_client_;

  const PasswordSuggestionGenerator suggestion_generator_;

  // If not null then it will be called in destructor.
  base::OnceClosure deletion_callback_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANUAL_FALLBACK_FLOW_H_
