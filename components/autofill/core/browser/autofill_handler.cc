// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_handler.h"

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

// Set a conservative upper bound on the number of forms we are willing to
// cache, simply to prevent unbounded memory consumption.
const size_t kAutofillHandlerMaxFormCacheSize = 100;

// Returns the AutofillField* corresponding to |field| in |form| or nullptr,
// if not found.
AutofillField* FindAutofillFillField(const FormStructure& form,
                                     const FormFieldData& field) {
  for (const auto& cur_field : form) {
    if (cur_field->SameFieldAs(field)) {
      return cur_field.get();
    }
  }
  return nullptr;
}

// Returns true if |live_form| does not match |cached_form|.
bool CachedFormNeedsUpdate(const FormData& live_form,
                           const FormStructure& cached_form) {
  if (live_form.fields.size() != cached_form.field_count())
    return true;

  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    if (!cached_form.field(i)->SameFieldAs(live_form.fields[i]))
      return true;
  }

  return false;
}

}  // namespace

using base::TimeTicks;

AutofillHandler::AutofillHandler(AutofillDriver* driver,
                                 LogManager* log_manager)
    : driver_(driver), log_manager_(log_manager) {}

AutofillHandler::~AutofillHandler() = default;

void AutofillHandler::OnFormSubmitted(const FormData& form,
                                      bool known_success,
                                      mojom::SubmissionSource source) {
  if (IsValidFormData(form))
    OnFormSubmittedImpl(form, known_success, source);
}

void AutofillHandler::OnFormsSeen(const std::vector<FormData>& forms,
                                  const base::TimeTicks timestamp) {
  if (!IsValidFormDataVector(forms) || !driver_->RendererIsAvailable())
    return;

  // This should be called even forms is empty, AutofillProviderAndroid uses
  // this event to detect form submission.
  if (!ShouldParseForms(forms, timestamp))
    return;

  if (forms.empty())
    return;

  std::vector<const FormData*> new_forms;
  for (const FormData& form : forms) {
    const auto parse_form_start_time = AutofillTickClock::NowTicks();
    FormStructure* cached_form_structure =
        FindCachedFormByRendererId(form.unique_renderer_id);
    // Autofill used to ignore cache hits for non-credit-card forms. The
    // motivation behind this is probably to have credit-card forms preserve
    // their original signature, whereas non-credit-card forms would use the
    // most recent form signature. Ignoring cache hits however appears to be
    // part of breaking profile imports and voting for dynamic forms. See
    // crbug/1091401#c15 for details.
    //
    // Therefore, if the kAutofillKeepInitialFormValuesInCache experiment is
    // enabled, we do not ignore cache hits, but in those cases where the old
    // code would have ignored the cache hit we update the FormStructure's
    // FormSignature.
    // Otherwise, if the experiment disabled, we just ignore the cache hit.
    //
    // TODO(crbug.com/1100231) Clean up when experiment is complete.
    const bool kOldBehavior = !base::FeatureList::IsEnabled(
        features::kAutofillKeepInitialFormValuesInCache);
    bool update_form_signature = false;
    if (cached_form_structure) {
      for (const FormType& form_type : cached_form_structure->GetFormTypes()) {
        if (form_type != CREDIT_CARD_FORM) {
          update_form_signature = true;
          if (kOldBehavior)
            cached_form_structure = nullptr;
          break;
        }
      }
    }

    FormStructure* form_structure = ParseForm(form, cached_form_structure);
    if (!form_structure)
      continue;
    DCHECK(form_structure);

    if (update_form_signature && !kOldBehavior)
      form_structure->set_form_signature(CalculateFormSignature(form));

    new_forms.push_back(&form);
    AutofillMetrics::LogParseFormTiming(AutofillTickClock::NowTicks() -
                                        parse_form_start_time);
  }

  if (new_forms.empty())
    return;
  OnFormsParsed(new_forms, timestamp);
}

void AutofillHandler::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           const TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidChangeImpl(form, field, transformed_box, timestamp);
}

void AutofillHandler::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnTextFieldDidScrollImpl(form, field, transformed_box);
}

void AutofillHandler::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnSelectControlDidChangeImpl(form, field, transformed_box);
}

void AutofillHandler::OnQueryFormFieldAutofill(
    int query_id,
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    bool autoselect_first_suggestion) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnQueryFormFieldAutofillImpl(query_id, form, field, transformed_box,
                               autoselect_first_suggestion);
}

void AutofillHandler::OnFocusOnFormField(const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  gfx::RectF transformed_box =
      driver_->TransformBoundingBoxToViewportCoordinates(bounding_box);

  OnFocusOnFormFieldImpl(form, field, transformed_box);
}

void AutofillHandler::SendFormDataToRenderer(
    int query_id,
    AutofillDriver::RendererFormDataAction action,
    const FormData& data) {
  driver_->SendFormDataToRenderer(query_id, action, data);
}

bool AutofillHandler::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Maybe find an existing FormStructure that corresponds to |form|.
  FormStructure* cached_form =
      FindCachedFormByRendererId(form.unique_renderer_id);
  if (cached_form) {
    DCHECK(cached_form);
    if (!CachedFormNeedsUpdate(form, *cached_form)) {
      // There is no data to return if there are no auto-fillable fields.
      if (!cached_form->autofill_count())
        return false;

      // Return the cached form and matching field, if any.
      *form_structure = cached_form;
      *autofill_field = FindAutofillFillField(**form_structure, field);
      return *autofill_field != nullptr;
    }
  }

  // The form is new or updated, parse it and discard |cached_form|.
  // i.e., |cached_form| is no longer valid after this call.
  *form_structure = ParseForm(form, cached_form);
  if (!*form_structure)
    return false;

  // Annotate the updated form with its predicted types.
  driver()->SendAutofillTypePredictionsToRenderer({*form_structure});

  // There is no data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = FindAutofillFillField(**form_structure, field);
  return *autofill_field != nullptr;
}

size_t AutofillHandler::FindCachedFormsBySignature(
    FormSignature form_signature,
    std::vector<FormStructure*>* form_structures) const {
  size_t hits_num = 0;
  for (const auto& p : form_structures_) {
    if (p.second->form_signature() == form_signature) {
      ++hits_num;
      if (form_structures)
        form_structures->push_back(p.second.get());
    }
  }
  return hits_num;
}

FormStructure* AutofillHandler::FindCachedFormByRendererId(
    FormRendererId form_renderer_id) const {
  auto it = form_structures_.find(form_renderer_id);
  return it != form_structures_.end() ? it->second.get() : nullptr;
}

FormStructure* AutofillHandler::ParseForm(const FormData& form,
                                          const FormStructure* cached_form) {
  if (form_structures_.size() >= kAutofillHandlerMaxFormCacheSize) {
    if (log_manager_) {
      log_manager_->Log() << LoggingScope::kAbortParsing
                          << LogMessage::kAbortParsingTooManyForms << form;
    }
    return nullptr;
  }

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->ParseFieldTypesFromAutocompleteAttributes();
  if (!form_structure->ShouldBeParsed(log_manager_))
    return nullptr;

  if (cached_form) {
    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(*cached_form,
                                      /*should_keep_cached_value=*/true,
                                      /*only_server_and_autofill_state=*/true);
    if (observer_for_testing_)
      observer_for_testing_->OnFormParsed();

    if (form_structure.get()->value_from_dynamic_change_form())
      value_from_dynamic_change_form_ = true;
  }

  form_structure->set_page_language(GetPageLanguage());

  form_structure->DetermineHeuristicTypes(log_manager_);

  // Hold the parsed_form_structure we intend to return. We can use this to
  // reference the form_signature when transferring ownership below.
  FormStructure* parsed_form_structure = form_structure.get();

  // Ownership is transferred to |form_structures_| which maintains it until
  // the form is parsed again or the AutofillHandler is destroyed.
  //
  // Note that this insert/update takes ownership of the new form structure
  // and also destroys the previously cached form structure.
  form_structures_[parsed_form_structure->unique_renderer_id()] =
      std::move(form_structure);

  return parsed_form_structure;
}

std::string AutofillHandler::GetPageLanguage() const {
  return std::string();
}

void AutofillHandler::Reset() {
  form_structures_.clear();
}

}  // namespace autofill
