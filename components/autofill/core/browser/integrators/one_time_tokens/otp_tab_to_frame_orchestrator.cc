// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_tab_to_frame_orchestrator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/metrics/otp_form_event_logger.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_field_detector.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_metrics_tracker.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

OtpTabToFrameOrchestrator::OtpTabToFrameOrchestrator(
    BrowserAutofillManager* owner,
    OtpCoordinator* coordinator)
    : owner_(CHECK_DEREF(owner)), coordinator_(CHECK_DEREF(coordinator)) {
  if (!owner_->driver().IsEmbedded()) {
    autofill_manager_observation_.Observe(owner);
    coordinator_->RegisterObserver(this);
  }
}

OtpTabToFrameOrchestrator::~OtpTabToFrameOrchestrator() {
  if (autofill_manager_observation_.IsObserving()) {
    coordinator_->OnFrameBecameInactive(owner_->driver().GetFrameToken());
    coordinator_->UnregisterObserver(this);
  }
}

void OtpTabToFrameOrchestrator::GetOtpSuggestions(
    const FormStructure& form,
    const FormFieldData& field,
    GetOtpSuggestionsCallback callback) {
  const AutofillField* autofill_field = form.GetFieldById(field.global_id());
  if (!autofill_field) {
    std::move(callback).Run({});
    return;
  }

  last_queried_otp_field_ = field.global_id();
  std::move(callback).Run(coordinator_->GetAvailableTokens(
      BuildOtpTarget(*owner_, form, *autofill_field)));
}

void OtpTabToFrameOrchestrator::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState previous,
    AutofillManager::LifecycleState new_state) {
  if (new_state != AutofillManager::LifecycleState::kActive) {
    coordinator_->OnFrameBecameInactive(owner_->driver().GetFrameToken());
    return;
  }
  coordinator_->StartListeningForEligibleTargets();
}

void OtpTabToFrameOrchestrator::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  for (FormGlobalId form_id : removed_forms) {
    coordinator_->StopListeningForForm(form_id);
  }
}

void OtpTabToFrameOrchestrator::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form_id,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  if (!owner_->driver().IsActive()) {
    return;
  }

  const FormStructure* form = owner_->FindCachedFormById(form_id);
  if (!form) {
    return;
  }
  if (!OtpFieldDetector::IsOtpForm(*form)) {
    coordinator_->StopListeningForForm(form_id);
    return;
  }

  LOG_AF(owner_->client().GetCurrentLogManager())
      << LoggingScope::kOneTimeTokens << "OTP field detected in web form."
      << Br{} << "Form ID: " << form_id;

  std::vector<FieldGlobalId> otp_field_ids;
  for (const auto& field : form->fields()) {
    if (OtpFieldDetector::IsOtpField(*form, *field)) {
      otp_field_ids.push_back(field->global_id());
      coordinator_->StartListeningForOtp(
          BuildOtpTarget(*owner_, *form, *field));
    }
  }
  coordinator_->StopListeningForForm(
      form_id, base::flat_set<FieldGlobalId>(otp_field_ids));

  if (OtpMetricsTracker* tracker = owner_->client().GetOtpMetricsTracker()) {
    tracker->OnOtpFieldDetected(form_id, std::move(otp_field_ids), *owner_);
  }
}

void OtpTabToFrameOrchestrator::OnBeforeFocusOnFormField(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (last_queried_otp_field_ != field_id) {
    last_queried_otp_field_.reset();
  }
  if (owner_->driver().IsActive()) {
    coordinator_->OnFocusChanged(owner_->driver().GetFrameToken(), form_id,
                                 field_id);
  }
}

void OtpTabToFrameOrchestrator::OnBeforeFocusOnNonFormField(
    AutofillManager& manager) {
  last_queried_otp_field_.reset();
  if (owner_->driver().IsActive()) {
    coordinator_->OnFocusChanged(owner_->driver().GetFrameToken(), std::nullopt,
                                 std::nullopt);
  }
}

void OtpTabToFrameOrchestrator::OnSuggestionsShown(
    AutofillManager& manager,
    base::span<const Suggestion> suggestions) {
  if (!last_queried_otp_field_) {
    return;
  }
  auto it = std::ranges::find_if(suggestions, [](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kOneTimePasswordEntry ||
           suggestion.type == SuggestionType::kGmailOneTimePasswordEntry;
  });
  if (it != suggestions.end()) {
    coordinator_->OnOtpSuggestionsShown(*last_queried_otp_field_,
                                        base::UTF16ToUTF8(it->main_text.value));
  }
}

void OtpTabToFrameOrchestrator::OnBeforeFormSubmitted(AutofillManager& manager,
                                                      const FormData& form) {
  coordinator_->OnOtpFormSubmitted(form);
}

BrowserAutofillManager& OtpTabToFrameOrchestrator::GetBrowserAutofillManager()
    const {
  return *owner_;
}

void OtpTabToFrameOrchestrator::OnOtpAvailable(const OtpSuggestionData&) {
  owner_->GetOtpFormEventLogger().OnOtpAvailable();
}

void OtpTabToFrameOrchestrator::TriggerSuggestions(FieldGlobalId field_id) {
  last_queried_otp_field_ = field_id;
  owner_->driver().RendererShouldTriggerSuggestions(
      field_id,
      AutofillSuggestionTriggerSource::kGmailOneTimePasswordAvailable);
}

}  // namespace autofill
