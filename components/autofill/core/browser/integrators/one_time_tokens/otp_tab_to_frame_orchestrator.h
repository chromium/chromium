// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TAB_TO_FRAME_ORCHESTRATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TAB_TO_FRAME_ORCHESTRATOR_H_

#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_coordinator.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_manager.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class BrowserAutofillManager;
class FormData;
class FormFieldData;
class FormStructure;
struct OtpSuggestionData;
struct Suggestion;

// Per-frame orchestrator owned by `BrowserAutofillManager` that connects a
// frame's `AutofillManager` lifecycle and form events to the tab-scoped
// `OtpCoordinator` (owned by `AutofillClient`). Skips embedded frames.
class OtpTabToFrameOrchestrator : public OtpManager,
                                  public AutofillManager::Observer,
                                  public OtpCoordinator::Observer {
 public:
  // `owner` and `coordinator` must be non-null and outlive this orchestrator.
  // For non-embedded frames, registers `this` with `coordinator` on
  // construction and unregisters it on destruction.
  OtpTabToFrameOrchestrator(BrowserAutofillManager* owner,
                            OtpCoordinator* coordinator);
  OtpTabToFrameOrchestrator(const OtpTabToFrameOrchestrator&) = delete;
  OtpTabToFrameOrchestrator& operator=(const OtpTabToFrameOrchestrator&) =
      delete;
  ~OtpTabToFrameOrchestrator() override;

  // OtpManager:
  void GetOtpSuggestions(const FormStructure& form,
                         const FormFieldData& field,
                         GetOtpSuggestionsCallback callback) override;

  // AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState previous,
      AutofillManager::LifecycleState new_state) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;
  void OnFieldTypesDetermined(AutofillManager& manager,
                              FormGlobalId form_id,
                              AutofillManager::Observer::FieldTypeSource source,
                              bool small_forms_were_parsed) override;
  void OnBeforeFocusOnFormField(AutofillManager& manager,
                                FormGlobalId form_id,
                                FieldGlobalId field_id) override;
  void OnBeforeFocusOnNonFormField(AutofillManager& manager) override;
  void OnSuggestionsShown(AutofillManager& manager,
                          base::span<const Suggestion> suggestions) override;
  void OnBeforeFormSubmitted(AutofillManager& manager,
                             const FormData& form) override;

  // OtpCoordinator::Observer:
  BrowserAutofillManager& GetBrowserAutofillManager() const override;
  void OnOtpAvailable(const OtpSuggestionData& data) override;
  void TriggerSuggestions(FieldGlobalId field_id) override;

 private:
  const raw_ref<BrowserAutofillManager> owner_;
  const raw_ref<OtpCoordinator> coordinator_;
  std::optional<FieldGlobalId> last_queried_otp_field_;

  base::ScopedObservation<BrowserAutofillManager, AutofillManager::Observer>
      autofill_manager_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_OTP_TAB_TO_FRAME_ORCHESTRATOR_H_
