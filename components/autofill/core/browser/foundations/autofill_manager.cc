// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_manager.h"

#include <algorithm>
#include <optional>
#include <ranges>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_server_prediction.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/language_detection/core/constants.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/autofill/core/browser/ml_model/model_predictions.h"
#endif

namespace autofill {

namespace {

// ParsingCallback() and NotifyObserversCallback() assemble the reply callback
// for ParseFormAsync().
//
// An event
//   AutofillManager::OnFoo(const FormData& form, args...)
// is handled by
// asynchronously parsing the form and then calling
//   AutofillManager::OnFooImpl(const FormData& form, args...)
// unless the AutofillManager has been destructed or reset in the meantime.
//
// For some events, AutofillManager::Observer::On{Before,After}Foo() must be
// called before/after AutofillManager::OnFooImpl().
//
// The corresponding callback for ParseFormAsync() is assembled by
//   ParsingCallback(&AutofillManager::OnFooImpl, ...)
//       .Then(NotifyObserversCallback(&Observer::OnAfterFoo, ...))
template <typename Functor, typename... Args>
base::OnceCallback<AutofillManager&(AutofillManager&, const FormData&)>
ParsingCallback(Functor&& functor, Args&&... args) {
  return base::BindOnce(
      [](Functor&& functor, std::remove_reference_t<Args&&>... args,
         AutofillManager& self, const FormData& form) -> AutofillManager& {
        std::invoke(std::forward<Functor>(functor), self, form,
                    std::forward<Args>(args)...);
        return self;
      },
      std::forward<Functor>(functor), std::forward<Args>(args)...);
}

// See ParsingCallback().
template <typename Functor, typename... Args>
[[nodiscard]] base::OnceCallback<void(AutofillManager&)>
NotifyObserversCallback(Functor&& functor, Args&&... args) {
  return base::BindOnce(
      [](Functor&& functor, std::remove_reference_t<Args&&>... args,
         AutofillManager& self) {
        self.NotifyObservers(std::forward<Functor>(functor),
                             std::forward<Args>(args)...);
      },
      std::forward<Functor>(functor), std::forward<Args>(args)...);
}

// Returns true if `live_form` has changed compared to `cached_form` in aspects
// that may affect type predictions.
bool NeedsReparse(const FormData& live_form, const FormStructure& cached_form) {
  return live_form.version() >= cached_form.version() &&
         !std::ranges::equal(
             live_form.fields(), cached_form.fields(),
             [](const FormFieldData& f,
                const std::unique_ptr<AutofillField>& g) {
               return FormFieldData::IdenticalAndEquivalentDomElements(
                   f, *g, {FormFieldData::Exclusion::kValue});
             });
}

bool IsCreditCardFormForSignaturePurposes(const FormStructure& form_structure) {
  return form_structure.GetFormTypes() ==
         DenseSet<FormType>{FormType::kCreditCardForm};
}

}  // namespace

// Form parsing happens asynchronously. This struct holds the necessary context.
// The AsyncContext can be passed to any sequence and its members can be
// accessed on that sequence.
struct AutofillManager::AsyncContext {
  AsyncContext(AutofillManager& manager, std::vector<FormData> forms)
      : forms(std::move(forms)),
        country_code(manager.client().GetVariationConfigCountryCode()),
        current_page_language(manager.GetCurrentPageLanguage()),
        log_manager(IsLoggingActive(manager.log_manager())
                        ? LogManager::CreateBuffering()
                        : nullptr) {}

  std::vector<FormData> forms;
  std::vector<RegexPredictions> regex_predictions;
  std::vector<ModelPredictions> autofill_predictions;
  std::vector<ModelPredictions> password_manager_predictions;
  GeoIpCountryCode country_code;
  LanguageCode current_page_language;
  std::unique_ptr<BufferingLogManager> log_manager;
};

AutofillManager::AutofillManager(AutofillDriver* driver)
    : driver_(CHECK_DEREF(driver)) {
  if (auto* translate_driver = client().GetTranslateDriver()) {
    translate_observation_.Observe(translate_driver);
  }
}

AutofillManager::~AutofillManager() {
  translate_observation_.Reset();
}

void AutofillManager::OnAutofillDriverLifecycleStateChanged(
    LifecycleState old_state,
    LifecycleState new_state,
    base::PassKey<AutofillDriverFactory>) {
  DCHECK_NE(new_state, old_state);
  DCHECK_EQ(new_state, driver().GetLifecycleState());
  NotifyObservers(&Observer::OnAutofillManagerStateChanged, old_state,
                  new_state);
  if (new_state == LifecycleState::kPendingReset) {
    Reset();
  }
}

void AutofillManager::Reset() {
  parsing_weak_ptr_factory_.InvalidateWeakPtrs();
  form_structures_.clear();
}

void AutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (!base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)) {
    return;
  }
  if (details.adopted_language == language_detection::kUnknownLanguageCode ||
      !driver_->IsActive()) {
    return;
  }

  NotifyObservers(&Observer::OnBeforeLanguageDetermined);

  // Wait for ongoing parsing operations to finish, so `form_structures_` is
  // up to date.
  AfterParsingFinishesDeprecated(base::BindOnce([](base::WeakPtr<
                                                    AutofillManager> self) {
    if (!self) {
      return;
    }
    std::vector<FormData> forms;
    forms.reserve(self->form_structures_.size());
    for (const auto& [id, form_structure] : self->form_structures_) {
      forms.push_back(form_structure->ToFormData());
    }
    self->ParseFormsAsync(
        forms, base::BindOnce([](AutofillManager& self,
                                 const std::vector<FormData>& parsed_forms) {
          self.NotifyObservers(&Observer::OnAfterLanguageDetermined);
        }));
  })).Run(GetWeakPtr());
}

void AutofillManager::OnTranslateDriverDestroyed(
    translate::TranslateDriver* translate_driver) {
  translate_observation_.Reset();
}

LanguageCode AutofillManager::GetCurrentPageLanguage() {
  const translate::LanguageState* language_state = client().GetLanguageState();
  if (!language_state) {
    return LanguageCode();
  }
  return LanguageCode(language_state->current_language());
}

void AutofillManager::OnDidAutofillForm(const FormData& form) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeDidAutofillForm, form.global_id());
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnDidAutofillFormImpl)
                .Then(NotifyObserversCallback(&Observer::OnAfterDidAutofillForm,
                                              form.global_id())));
}

void AutofillManager::OnFormSubmitted(const FormData& form,
                                      const mojom::SubmissionSource source) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeFormSubmitted, form);
  OnFormSubmittedImpl(form, source);
  NotifyObservers(&Observer::OnAfterFormSubmitted, form);
}

void AutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  auto erase_removed_forms = [&] {
    // Erase forms that have been removed from the DOM. This prevents
    // |form_structures_| from growing up its upper bound
    // kAutofillManagerMaxFormCacheSize.
    for (FormGlobalId removed_form : removed_forms) {
      form_structures_.erase(removed_form);
    }
  };

  if (!IsValidFormDataVector(updated_forms) || !ShouldParseForms()) {
    NotifyObservers(&Observer::OnBeforeFormsSeen, std::vector<FormGlobalId>{},
                    removed_forms);
    erase_removed_forms();
    NotifyObservers(&Observer::OnAfterFormsSeen, std::vector<FormGlobalId>{},
                    removed_forms);
    return;
  }

  NotifyObservers(&Observer::OnBeforeFormsSeen,
                  base::ToVector(updated_forms, &FormData::global_id),
                  removed_forms);
  erase_removed_forms();

  auto ProcessParsedForms = [](std::vector<FormGlobalId> removed_forms,
                               AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty()) {
      self.OnFormsParsed(parsed_forms);
    }
    self.NotifyObservers(&Observer::OnAfterFormsSeen,
                         base::ToVector(parsed_forms, &FormData::global_id),
                         removed_forms);
  };
  ParseFormsAsync(updated_forms,
                  base::BindOnce(ProcessParsedForms, std::move(removed_forms)));
}

void AutofillManager::OnFormsParsed(const std::vector<FormData>& forms) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  std::vector<raw_ptr<const FormStructure, VectorExperimental>> queryable_forms;
  for (const FormData& form : forms) {
    // The FormStructure might not exist if the form cache hit its capacity of
    // `kAutofillManagerMaxFormCacheSize` and due to race conditions the initial
    // check in ParseFormsAsync() was passed.
    const FormStructure* form_structure = FindCachedFormById(form.global_id());
    if (!form_structure) {
      continue;
    }

    // Configure the query encoding for this form and add it to the appropriate
    // collection of forms: queryable vs non-queryable.
    if (ShouldBeQueried(*form_structure)) {
      queryable_forms.push_back(form_structure);
    }

    OnFormProcessed(form, *form_structure);
  }

  if (base::FeatureList::IsEnabled(features::debug::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIdsInAllFrames();
  }

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty()) {
    NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
    // If language detection is currently reparsing the form, wait until the
    // server response is processed, to ensure server predictions are not lost.
    client().GetCrowdsourcingManager().StartQueryRequest(
        queryable_forms, driver().GetIsolationInfo(),
        AfterParsingFinishesDeprecated(base::BindOnce(
            &AutofillManager::OnLoadedServerPredictions, GetWeakPtr())));
  }
}

void AutofillManager::OnCaretMovedInFormField(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const gfx::Rect& caret_bounds) {
  if (!IsValidFormData(form)) {
    return;
  }
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  NotifyObservers(&Observer::OnBeforeCaretMovedInFormField, form.global_id(),
                  field_id, field.selected_text(), caret_bounds);
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnCaretMovedInFormFieldImpl,
                            field_id, caret_bounds)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterCaretMovedInFormField, form.global_id(),
                    field_id, field.selected_text(), caret_bounds)));
}

void AutofillManager::OnTextFieldValueChanged(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const base::TimeTicks timestamp) {
  if (!IsValidFormData(form)) {
    return;
  }
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  NotifyObservers(&Observer::OnBeforeTextFieldValueChanged, form.global_id(),
                  field_id);
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnTextFieldValueChangedImpl,
                            field_id, timestamp)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterTextFieldValueChanged, form.global_id(),
                    field_id, field.value())));
}

void AutofillManager::OnTextFieldDidScroll(const FormData& form,
                                           const FieldGlobalId& field_id) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeTextFieldDidScroll, form.global_id(),
                  field_id);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnTextFieldDidScrollImpl, field_id)
          .Then(NotifyObserversCallback(&Observer::OnAfterTextFieldDidScroll,
                                        form.global_id(), field_id)));
}

void AutofillManager::OnSelectControlSelectionChanged(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeSelectControlSelectionChanged,
                  form.global_id(), field_id);
  ParseFormAsync(
      form, ParsingCallback(
                &AutofillManager::OnSelectControlSelectionChangedImpl, field_id)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterSelectControlSelectionChanged,
                    form.global_id(), field_id)));
}

void AutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    std::optional<PasswordSuggestionRequest> password_request) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeAskForValuesToFill, form.global_id(),
                  field_id, form);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnAskForValuesToFillImpl, field_id,
                      caret_bounds, trigger_source, std::move(password_request))
          .Then(NotifyObserversCallback(&Observer::OnAfterAskForValuesToFill,
                                        form.global_id(), field_id)));
}

void AutofillManager::OnFocusOnFormField(const FormData& form,
                                         const FieldGlobalId& field_id) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeFocusOnFormField, form.global_id(),
                  field_id);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnFocusOnFormFieldImpl, field_id)
          .Then(NotifyObserversCallback(&Observer::OnAfterFocusOnFormField,
                                        form.global_id(), field_id)));
}

void AutofillManager::OnFocusOnNonFormField() {
  NotifyObservers(&Observer::OnBeforeFocusOnNonFormField);
  OnFocusOnNonFormFieldImpl();
  NotifyObservers(&Observer::OnAfterFocusOnNonFormField);
}

void AutofillManager::OnDidEndTextFieldEditing() {
  OnDidEndTextFieldEditingImpl();
}

void AutofillManager::OnHidePopup() {
  OnHidePopupImpl();
}

void AutofillManager::OnSuggestionsHidden() {
  // If the unmask prompt is shown, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (ShouldClearPreviewedForm()) {
    driver().RendererShouldClearPreviewedForm();
  }
  NotifyObservers(&Observer::OnSuggestionsHidden);
}

void AutofillManager::OnSelectFieldOptionsDidChange(
    const FormData& form,
    const FieldGlobalId& field_id) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeSelectFieldOptionsDidChange,
                  form.global_id());
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnSelectFieldOptionsDidChangeImpl,
                            field_id)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterSelectFieldOptionsDidChange,
                    form.global_id())));
}

void AutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeJavaScriptChangedAutofilledValue,
                  form.global_id(), field_id);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnJavaScriptChangedAutofilledValueImpl,
                      field_id, old_value)
          .Then(NotifyObserversCallback(
              &Observer::OnAfterJavaScriptChangedAutofilledValue,
              form.global_id(), field_id)));
}

bool AutofillManager::GetCachedFormAndField(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    FormStructure** form_structure,
    AutofillField** autofill_field) const {
  FormStructure* cached_form = FindCachedFormById(form_id);
  if (!cached_form) {
    return false;
  }
  *form_structure = cached_form;
  *autofill_field = cached_form->GetFieldById(field_id);
  return *autofill_field != nullptr;
}

size_t AutofillManager::FindCachedFormsBySignature(
    FormSignature form_signature,
    std::vector<raw_ptr<FormStructure, VectorExperimental>>* form_structures)
    const {
  size_t hits_num = 0;
  for (const auto& [form_id, form_structure] : form_structures_) {
    if (form_structure->form_signature() == form_signature) {
      ++hits_num;
      if (form_structures) {
        form_structures->push_back(form_structure.get());
      }
    }
  }
  return hits_num;
}

FormStructure* AutofillManager::FindCachedFormById(FormGlobalId form_id) const {
  auto it = form_structures_.find(form_id);
  return it != form_structures_.end() ? it->second.get() : nullptr;
}

FormStructure* AutofillManager::FindCachedFormById(
    FieldGlobalId field_id) const {
  for (const auto& [form_id, form_structure] : form_structures_) {
    if (std::ranges::any_of(*form_structure, [&](const auto& field) {
          return field->global_id() == field_id;
        })) {
      return form_structure.get();
    }
  }
  return nullptr;
}

bool AutofillManager::CanShowAutofillUi() const {
  return driver_->CanShowAutofillUi();
}

void AutofillManager::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool success)> form_extraction_finished_callback) {
  driver_->TriggerFormExtractionInAllFrames(
      std::move(form_extraction_finished_callback));
}

void AutofillManager::ReparseKnownForms() {
  std::vector<FormData> forms;
  forms.reserve(form_structures_.size());
  for (const auto& [id, form_structure] : form_structures_) {
    forms.push_back(form_structure->ToFormData());
  }
  auto ProcessParsedForms = [](AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty()) {
      self.OnFormsParsed(parsed_forms);
    }
  };
  ParseFormsAsync(forms, base::BindOnce(ProcessParsedForms));
}

base::flat_map<FieldGlobalId, AutofillServerPrediction>
AutofillManager::GetServerPredictionsForForm(
    FormGlobalId form_id,
    const std::vector<FieldGlobalId>& field_ids) const {
  FormStructure* cached_form = FindCachedFormById(form_id);
  if (!cached_form) {
    return {};
  }
  return cached_form->GetServerPredictions(field_ids);
}

base::flat_map<FieldGlobalId, FieldType>
AutofillManager::GetHeuristicPredictionForForm(
    HeuristicSource source,
    FormGlobalId form_id,
    const std::vector<FieldGlobalId>& field_ids) const {
  const FormStructure* const cached_form = FindCachedFormById(form_id);
  if (!cached_form) {
    return {};
  }
  return cached_form->GetHeuristicPredictions(source, field_ids);
}

void AutofillManager::ParseFormsAsync(
    const std::vector<FormData>& forms,
    base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
        callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync");

  // `num_managed_forms` is the number of forms that will be managed by this
  // AutofillManager after ParseFormsAsync() and its asynchronous callees have
  // finished.
  size_t num_managed_forms = form_structures_.size();

  // To be run on the main thread (accesses member variables).
  std::vector<FormData> parseable_forms;
  parseable_forms.reserve(forms.size());
  for (const FormData& form : forms) {
    bool is_new_form = !base::Contains(form_structures_, form.global_id());
    if (num_managed_forms + is_new_form > kAutofillManagerMaxFormCacheSize) {
      LOG_AF(log_manager()) << LoggingScope::kAbortParsing
                            << LogMessage::kAbortParsingTooManyForms << form;
      continue;
    }

    if (!ShouldBeParsed(form, log_manager())) {
      LogCurrentFieldTypes(&form);
      continue;
    }

    num_managed_forms += is_new_form;
    parseable_forms.push_back(form);
  }

  ParseFormsAsyncCommon(
      /*preserve_signatures=*/false, std::move(parseable_forms),
      std::move(callback));
}

void AutofillManager::ParseFormAsync(
    const FormData& form,
    base::OnceCallback<void(AutofillManager&, const FormData&)> callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormAsync");

  bool is_new_form = !base::Contains(form_structures_, form.global_id());
  if (form_structures_.size() + is_new_form >
      kAutofillManagerMaxFormCacheSize) {
    LOG_AF(log_manager()) << LoggingScope::kAbortParsing
                          << LogMessage::kAbortParsingTooManyForms << form;
    return;
  }

  if (!ShouldBeParsed(form, log_manager())) {
    LogCurrentFieldTypes(&form);
    // For Autocomplete, events need to be handled even for forms that cannot be
    // parsed.
    std::move(callback).Run(*this, form);
    return;
  }

  if (const FormStructure* const cached_form_structure =
          FindCachedFormById(form.global_id());
      cached_form_structure && !NeedsReparse(form, *cached_form_structure)) {
    UpdateFormCache(
        base::span_from_ref(form),
        /*context=*/std::nullopt,
        FormStructure::RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing,
        /*preserve_signatures=*/true);
    std::move(callback).Run(*this, std::move(form));
    return;
  }

  std::vector<FormData> forms;
  forms.push_back(std::move(form));
  ParseFormsAsyncCommon(
      /*preserve_signatures=*/true, std::move(forms),
      base::BindOnce(
          [](base::OnceCallback<void(AutofillManager&, const FormData&)>
                 callback,
             AutofillManager& manager, const std::vector<FormData>& forms) {
            CHECK_EQ(forms.size(), 1u);
            std::move(callback).Run(manager, forms.front());
          },
          std::move(callback)));
}

void AutofillManager::ParseFormsAsyncCommon(
    bool preserve_signatures,
    std::vector<FormData> forms,
    base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
        callback) {
  // To be run on a different task (must not access global or member
  // variables).
  auto run_heuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.RunHeuristics");
    context.regex_predictions.reserve(context.forms.size());
    for (const FormData& form : context.forms) {
      context.regex_predictions.push_back(DetermineRegexTypes(
          context.country_code, context.current_page_language, form,
          context.log_manager.get()));
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self, bool preserve_signatures,
         base::OnceCallback<void(AutofillManager&,
                                 const std::vector<FormData>&)> callback,
         AsyncContext context) {
        if (!self) {
          return;
        }
        CHECK_EQ(context.regex_predictions.size(), context.forms.size());
        self->UpdateFormCache(context.forms, context,
                              FormStructure::RetrieveFromCacheReason::
                                  kFormCacheUpdateAfterParsing,
                              preserve_signatures);
        for (const FormData& form : context.forms) {
          if (const FormStructure* const form_structure =
                  self->FindCachedFormById(form.global_id())) {
            self->LogCurrentFieldTypes(form_structure);
            self->NotifyObservers(
                &Observer::OnFieldTypesDetermined, form_structure->global_id(),
                Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
          }
        }
        if (context.log_manager && self->log_manager()) {
          context.log_manager->Flush(*self->log_manager());
        }
        std::move(callback).Run(*self, context.forms);
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), preserve_signatures,
      std::move(callback));

  // To be run on the main thread (accesses member variables).
  auto run_heuristics_and_update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         AsyncContext (*run_heuristics)(AsyncContext),
         base::OnceCallback<void(AsyncContext)> update_cache,
         AsyncContext context) {
        if (!self) {
          return;
        }
        self->parsing_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE, base::BindOnce(run_heuristics, std::move(context)),
            std::move(update_cache));
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), run_heuristics,
      std::move(update_cache));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Parsing happens in the following order:
  // (1) Running ML models (Autofill and Password Manager).
  // (2) Running heuristics (this ensures that rationalization and sectioning
  //     are done for the active Autofill predictions).
  // (3) Updating the form cache.
  RunMlModels(AsyncContext(*this, std::move(forms)),
              std::move(run_heuristics_and_update_cache));
#else
  std::move(run_heuristics_and_update_cache)
      .Run(AsyncContext(*this, std::move(forms)));
#endif
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Applies the Autofill and Password Manager ML models for field classification
// to `forms`. The model is executed on a background sequence.
// Calls `done_callback` upon completion on the UI sequence.
void AutofillManager::RunMlModels(
    AsyncContext context,
    base::OnceCallback<void(AsyncContext)> done_callback) {
  // Runs the specified model and calls `response` with the results.
  // Otherwise runs `response` with the empty vector.
  // Called on the UI thread.
  auto run_model = [](HeuristicSource source,
                      base::WeakPtr<AutofillManager> manager,
                      base::OnceCallback<void(AsyncContext context,
                                              std::vector<ModelPredictions>)>
                          receive_predictions,
                      AsyncContext context) {
    if (!manager) {
      return;
    }
    auto* ml_handler = [&]() -> FieldClassificationModelHandler* {
      AutofillClient& client = manager->client();
      switch (source) {
        case HeuristicSource::kAutofillMachineLearning:
          return client.GetAutofillFieldClassificationModelHandler();
        case HeuristicSource::kPasswordManagerMachineLearning:
          return client.GetPasswordManagerFieldClassificationModelHandler();
        case HeuristicSource::kRegexes:
          break;
      }
      NOTREACHED();
    }();
    if (!ml_handler) {
      std::move(receive_predictions).Run(std::move(context), {});
      return;
    }
    LOG_AF(manager->client().GetCurrentLogManager())
        << LoggingScope::kParsing << LogMessage::kTriggeringClientsideModelFor
        << HeuristicSourceToString(source);
    manager->SubscribeToMlModelChanges(*ml_handler);
    GeoIpCountryCode country_code = context.country_code;
    std::vector<FormData> forms = context.forms;
    ml_handler->GetModelPredictionsForForms(
        std::move(forms), country_code,
        base::BindOnce(std::move(receive_predictions), std::move(context)));
  };

  // Stores the computed predictions.
  // Called on the UI thread.
  auto receive_predictions =
      [](AsyncContext context,
         std::vector<ModelPredictions> model_predictions) {
        if (model_predictions.empty()) {
          return context;
        }
        const HeuristicSource source = model_predictions.front().source();
        DCHECK(std::ranges::all_of(model_predictions,
                                   [source](const ModelPredictions& p) {
                                     return p.source() == source;
                                   }));
        switch (source) {
          case HeuristicSource::kAutofillMachineLearning:
            context.autofill_predictions = std::move(model_predictions);
            break;
          case HeuristicSource::kPasswordManagerMachineLearning:
            context.password_manager_predictions = std::move(model_predictions);
            break;
          case HeuristicSource::kRegexes:
            NOTREACHED();
        }
        return context;
      };

  // First run the Autofill model.
  run_model(
      HeuristicSource::kAutofillMachineLearning, GetWeakPtr(),
      base::BindOnce(receive_predictions)
          .Then(
              // Next run the Password Manager model.
              base::BindOnce(run_model,
                             HeuristicSource::kPasswordManagerMachineLearning,
                             GetWeakPtr(),
                             base::BindOnce(receive_predictions)
                                 .Then(
                                     // Then finish.
                                     std::move(done_callback)))),
      std::move(context));
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

// TODO(crbug.com/448144129): Remove once `kAutofillSynchronousAfterParsing`
// can be cleaned up.
template <typename... Args>
base::OnceCallback<void(Args...)>
AutofillManager::AfterParsingFinishesDeprecated(
    base::OnceCallback<void(Args...)> callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillSynchronousAfterParsing)) {
    return callback;
  }
  return base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(Args...)> callback, Args... args) {
        if (self) {
          self->parsing_task_runner_->PostTaskAndReply(
              FROM_HERE, base::DoNothing(),
              base::BindOnce(std::move(callback), std::forward<Args>(args)...));
        }
      },
      GetWeakPtr(), std::move(callback));
}

void AutofillManager::OnLoadedServerPredictions(
    std::optional<AutofillCrowdsourcingManager::QueryResponse> response) {
  absl::Cleanup on_after_loaded_server_predictions = [this] {
    NotifyObservers(&Observer::OnAfterLoadedServerPredictions);
  };

  if (!response) {
    return;
  }

  // Get the current valid FormStructures represented by
  // `response->queried_form_signatures`.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> queried_forms;
  queried_forms.reserve(response->queried_form_signatures.size());
  for (const auto& form_signature : response->queried_form_signatures) {
    FindCachedFormsBySignature(form_signature, &queried_forms);
  }

  // Each form signature in |queried_form_signatures| is supposed to be unique,
  // and therefore appear only once. This ensures that
  // FindCachedFormsBySignature() produces an output without duplicates in the
  // forms.
  // TODO(crbug.com/40123827): |queried_forms| could be a set data structure;
  // their order should be irrelevant.
  DCHECK_EQ(queried_forms.size(),
            std::set<FormStructure*>(queried_forms.begin(), queried_forms.end())
                .size());

  // If there are no current forms corresponding to the queried signatures, drop
  // the query response.
  if (queried_forms.empty()) {
    return;
  }

  // Parse and store the server predictions.
  ParseServerPredictionsQueryResponse(
      std::move(response->response), queried_forms,
      response->queried_form_signatures, log_manager());

  OnLoadedServerPredictionsImpl(queried_forms);
  if (base::FeatureList::IsEnabled(features::debug::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIdsInAllFrames();
  }

  for (const raw_ptr<FormStructure, VectorExperimental> form : queried_forms) {
    form->RationalizeAndAssignSections(client().GetVariationConfigCountryCode(),
                                       GetCurrentPageLanguage(), log_manager());

    autofill_metrics::LogQualityMetricsBasedOnAutocomplete(
        *form, client().GetFormInteractionsUkmLogger(),
        driver().GetPageUkmSourceId());
    LogCurrentFieldTypes(form.get());

    NotifyObservers(&Observer::OnFieldTypesDetermined, form->global_id(),
                    Observer::FieldTypeSource::kAutofillServer);
  }
}

void AutofillManager::UpdateFormCache(
    base::span<const FormData> forms,
    base::optional_ref<const AsyncContext> context,
    FormStructure::RetrieveFromCacheReason reason,
    bool preserve_signatures) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.UpdateCache");

  auto apply_predictions = [](FormStructure& form_structure,
                              const AsyncContext& context, size_t i) {
    if (!context.autofill_predictions.empty()) {
      context.autofill_predictions[i].ApplyTo(form_structure.fields());
    }
    if (!context.password_manager_predictions.empty()) {
      context.password_manager_predictions[i].ApplyTo(form_structure.fields());
    }
    if (!context.regex_predictions.empty()) {
      context.regex_predictions[i].ApplyTo(form_structure.fields());
    }
    form_structure.RationalizeAndAssignSections(context.country_code,
                                                context.current_page_language,
                                                context.log_manager.get());
  };

  for (size_t i = 0; i < forms.size(); ++i) {
    const FormStructure* const cached_form_structure =
        FindCachedFormById(forms[i].global_id());
    const bool is_new_form = !cached_form_structure;
    if (form_structures_.size() + is_new_form >
        kAutofillManagerMaxFormCacheSize) {
      LOG_AF(log_manager())
          << LoggingScope::kAbortParsing
          << LogMessage::kAbortParsingTooManyForms << forms[i];
      continue;
    }

    if (is_new_form) {
      DCHECK(context);
      auto form_structure = std::make_unique<FormStructure>(forms[i]);
      if (context) {
        apply_predictions(*form_structure, *context, i);
      }
      form_structures_[forms[i].global_id()] = std::move(form_structure);
      DCHECK_LE(form_structures_.size(), kAutofillManagerMaxFormCacheSize);
      continue;
    }

    auto form_structure = std::make_unique<FormStructure>(forms[i]);
    form_structure->RetrieveFromCache(*cached_form_structure, reason);
    if (context) {
      apply_predictions(*form_structure, *context, i);
    }

    if (!preserve_signatures &&
        !IsCreditCardFormForSignaturePurposes(*cached_form_structure)) {
      // Not updating signatures of credit card forms is legacy behaviour. We
      // believe that the signatures are kept stable for voting purposes.
      // Credit card forms are those which contain only credit card fields.
      // TODO(crbug.com/431754194): Investigate making the behavior consistent
      // across all form types.
      form_structure->set_form_signature(CalculateFormSignature(forms[i]));
      form_structure->set_alternative_form_signature(
          CalculateAlternativeFormSignature(forms[i]));
      form_structure->set_structural_form_signature(
          CalculateStructuralFormSignature(forms[i]));
    }
    form_structures_[forms[i].global_id()] = std::move(form_structure);
  }
}

void AutofillManager::LogCurrentFieldTypes(
    std::variant<const FormData*, const FormStructure*> form) {
  std::unique_ptr<FormStructure> form_placeholder;

  // Retrieves the FormStructure for `form`. Since the FormStructure is needed
  // only if logging is enabled, we keep this lazy.
  auto get_form_structure = [&]() -> const FormStructure& {
    return std::visit(
        absl::Overload{
            [&](const FormData* form) -> const FormStructure& {
              CHECK(form);
              if (const FormStructure* form_structure =
                      FindCachedFormById(form->global_id())) {
                return *form_structure;
              }
              if (!form_placeholder) {
                form_placeholder = std::make_unique<FormStructure>(*form);
              }
              return *form_placeholder;
            },
            [](const FormStructure* form_structure) -> const FormStructure& {
              return CHECK_DEREF(form_structure);
            }},
        form);
  };

  LogBuffer buffer(IsLoggingActive(log_manager()));
  LOG_AF(buffer) << get_form_structure();
  LOG_AF(log_manager()) << LoggingScope::kParsing << LogMessage::kParsedForms
                        << std::move(buffer);
  if (base::FeatureList::IsEnabled(
          features::debug::kAutofillShowTypePredictions)) {
    driver().SendTypePredictionsToRenderer(get_form_structure());
  }
}

void AutofillManager::SubscribeToMlModelChanges(
    FieldClassificationModelHandler& handler) {
  switch (handler.optimization_target()) {
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION:
      if (!autofill_model_change_subscription_) {
        autofill_model_change_subscription_ =
            handler.RegisterModelChangeCallback(base::BindRepeating(
                &AutofillManager::ReparseKnownForms, base::Unretained(this)));
      }
      break;
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION:
      if (!password_manager_model_change_subscription_) {
        password_manager_model_change_subscription_ =
            handler.RegisterModelChangeCallback(base::BindRepeating(
                &AutofillManager::ReparseKnownForms, base::Unretained(this)));
      }
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace autofill
