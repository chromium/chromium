// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>

#include "base/callback_list.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/autofill_server_prediction.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
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

bool IsCreditCardFormForSignaturePurposes(
    const FormStructure& form_structure,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) {
  return form_structure.GetFormTypes(ac_unrecognized_behavior) ==
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

  ParseFormsAsync(
      base::ToVector(form_structures_,
                     [](const auto& p) { return p.second->ToFormData(); }),
      base::BindOnce(
          [](AutofillManager& self, const std::vector<FormData>& parsed_forms) {
            self.NotifyObservers(&Observer::OnAfterLanguageDetermined);
          }));
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

void AutofillManager::SuppressAutomaticRefills(const FillId& fill_id) {
  SuppressAutomaticRefillsImpl(fill_id);
}

void AutofillManager::RequestRefill(const FillId& fill_id) {
  RequestRefillImpl(fill_id);
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
    const std::vector<FormGlobalId>& removed_form_ids) {
  auto erase_removed_forms = [&] {
    // Erase forms that have been removed from the DOM. This prevents
    // |form_structures_| from growing up its upper bound
    // kAutofillManagerMaxFormCacheSize.
    for (FormGlobalId removed_form : removed_form_ids) {
      form_structures_.erase(removed_form);
    }
  };

  if (!IsValidFormDataVector(updated_forms) || !ShouldParseForms()) {
    NotifyObservers(&Observer::OnBeforeFormsSeen, std::vector<FormGlobalId>{},
                    removed_form_ids);
    erase_removed_forms();
    NotifyObservers(&Observer::OnAfterFormsSeen, std::vector<FormGlobalId>{},
                    removed_form_ids);
    return;
  }

  std::vector<FormGlobalId> updated_form_ids =
      base::ToVector(updated_forms, &FormData::global_id);
  NotifyObservers(&Observer::OnBeforeFormsSeen, updated_form_ids,
                  removed_form_ids);
  erase_removed_forms();

  // TODO(crbug.com/470949499): Remove this timestamp once
  // features::kAutofillServerQueryPredictionsEarly is launched.
  // The timestamp is used to measure the time elapsed between OnFormsSeen() and
  // the server predictions response.
  const base::TimeTicks forms_seen_timestamp = base::TimeTicks::Now();
  if (base::FeatureList::IsEnabled(
          features::kAutofillServerQueryPredictionsEarly)) {
    QueryServerPredictions(updated_forms, forms_seen_timestamp);
  }

  auto process_parsed_forms = base::BindOnce(
      [](std::vector<FormGlobalId> updated_form_ids,
         std::vector<FormGlobalId> removed_form_ids,
         base::TimeTicks forms_seen_timestamp, AutofillManager& self,
         const std::vector<FormData>& parsed_forms) {
        if (!parsed_forms.empty()) {
          self.OnFormsParsed(parsed_forms, forms_seen_timestamp);
        }
        if (!base::FeatureList::IsEnabled(
                features::kAutofillManagerFiresOnAfterFooIfCacheIsFull)) {
          updated_form_ids = base::ToVector(parsed_forms, &FormData::global_id);
        }
        self.NotifyObservers(&Observer::OnAfterFormsSeen, updated_form_ids,
                             removed_form_ids);
      },
      std::move(updated_form_ids), std::move(removed_form_ids),
      forms_seen_timestamp);

  ParseFormsAsync(updated_forms, std::move(process_parsed_forms));
}

void AutofillManager::QueryServerPredictions(
    base::span<const FormData> forms,
    base::TimeTicks form_seen_timestamp) {
  std::vector<FormData> queryable_forms;
  for (const FormData& form : forms) {
    if (ShouldBeQueried(form)) {
      queryable_forms.push_back(form);
    }
  }

  if (queryable_forms.empty()) {
    return;
  }

  NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
  // TODO(crbug.com/470949499): Consider changing the type of callback that
  // StartQueryRequest() expects to include the queried forms. This would allow
  // StartQueryRequest() to provide the queried forms to the callback
  // automatically, instead of passing them in separately here.
  auto on_loaded =
      base::BindOnce(&AutofillManager::OnLoadedServerPredictions, GetWeakPtr(),
                     queryable_forms, form_seen_timestamp);
  client().GetCrowdsourcingManager().StartQueryRequest(
      std::move(queryable_forms), driver().GetIsolationInfo(),
      std::move(on_loaded));
}

void AutofillManager::OnFormsParsed(const std::vector<FormData>& forms,
                                    base::TimeTicks form_seen_timestamp) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  std::vector<FormData> queryable_forms;
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
    // TODO(crbug.com/470949499): Remove this check and StartQueryRequest()
    // once features::kAutofillServerQueryPredictionsEarly is launched.
    if (ShouldBeQueried(form)) {
      queryable_forms.push_back(form);
    }

    OnFormProcessed(form, *form_structure);
  }

  if (base::FeatureList::IsEnabled(features::debug::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIdsInAllFrames();
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillServerQueryPredictionsEarly)) {
    return;
  }

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty()) {
    NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
    // If language detection is currently reparsing the form, wait until the
    // server response is processed, to ensure server predictions are not lost.
    auto on_loaded =
        base::BindOnce(&AutofillManager::OnLoadedServerPredictions,
                       GetWeakPtr(), queryable_forms, form_seen_timestamp);
    client().GetCrowdsourcingManager().StartQueryRequest(
        std::move(queryable_forms), driver().GetIsolationInfo(),
        std::move(on_loaded));
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
                  field.global_id());
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnTextFieldValueChangedImpl,
                      field.global_id(), timestamp)
          .Then(NotifyObserversCallback(&Observer::OnAfterTextFieldValueChanged,
                                        form.global_id(), field.global_id())));
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

const FormStructure* AutofillManager::FindCachedFormById(
    const FormGlobalId& form_id) const {
  auto it = form_structures_.find(form_id);
  return it != form_structures_.end() ? it->second.get() : nullptr;
}

const FormStructure* AutofillManager::FindCachedFormById(
    const FieldGlobalId& field_id) const {
  for (const auto& [form_id, form_structure] : form_structures_) {
    if (std::ranges::any_of(*form_structure, [&](const auto& field) {
          return field->global_id() == field_id;
        })) {
      return form_structure.get();
    }
  }
  return nullptr;
}

FormStructure* AutofillManager::FindCachedFormById(
    const FormGlobalId& form_id,
    const FormMutationPassKey& pass_key) {
  return const_cast<FormStructure*>(
      std::as_const(*this).FindCachedFormById(form_id));
}

void AutofillManager::ForEachCachedForm(
    base::FunctionRef<void(const FormStructure&)> fun) const {
  for (const auto& [form_id, form_structure] : form_structures_) {
    fun(*form_structure);
  }
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
  auto ProcessParsedForms = [](AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty()) {
      self.OnFormsParsed(parsed_forms, base::TimeTicks());
    }
  };
  ParseFormsAsync(
      base::ToVector(form_structures_,
                     [](const auto& p) { return p.second->ToFormData(); }),
      base::BindOnce(ProcessParsedForms));
}

base::flat_map<FieldGlobalId, AutofillServerPrediction>
AutofillManager::GetServerPredictionsForForm(
    FormGlobalId form_id,
    base::span<const FieldGlobalId> field_ids) const {
  const FormStructure* cached_form = FindCachedFormById(form_id);
  if (!cached_form) {
    return {};
  }
  return cached_form->GetServerPredictions(field_ids);
}

base::flat_map<FieldGlobalId, FieldType>
AutofillManager::GetHeuristicPredictionForForm(
    HeuristicSource source,
    FormGlobalId form_id,
    base::span<const FieldGlobalId> field_ids) const {
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
    bool is_new_form = !form_structures_.contains(form.global_id());
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

  bool is_new_form = !form_structures_.contains(form.global_id());
  if (form_structures_.size() + is_new_form >
      kAutofillManagerMaxFormCacheSize) {
    LOG_AF(log_manager()) << LoggingScope::kAbortParsing
                          << LogMessage::kAbortParsingTooManyForms << form;
    if (base::FeatureList::IsEnabled(
            features::kAutofillManagerFiresOnAfterFooIfCacheIsFull)) {
      std::move(callback).Run(*this, form);
    }
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
  auto run_heuristics = [](AsyncContext context, bool ignore_small_forms) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.RunHeuristics");
    context.regex_predictions.reserve(context.forms.size());
    for (const FormData& form : context.forms) {
      context.regex_predictions.push_back(DetermineRegexTypes(
          context.country_code, context.current_page_language, form,
          context.log_manager.get(), ignore_small_forms));
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self, bool preserve_signatures,
         base::OnceCallback<void(AutofillManager&,
                                 const std::vector<FormData>&)> callback,
         bool small_forms_were_parsed, AsyncContext context) {
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
                Observer::FieldTypeSource::kHeuristicsOrAutocomplete,
                small_forms_were_parsed);
          }
        }
        if (context.log_manager && self->log_manager()) {
          context.log_manager->Flush(*self->log_manager());
        }
        std::move(callback).Run(*self, context.forms);
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), preserve_signatures,
      std::move(callback),
      /*small_forms_were_parsed=*/client().IsTabInActorMode());

  // To be run on the main thread (accesses member variables).
  auto run_heuristics_and_update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         AsyncContext (*run_heuristics)(AsyncContext, bool),
         base::OnceCallback<void(AsyncContext)> update_cache,
         bool ignore_small_forms, AsyncContext context) {
        if (!self) {
          return;
        }
        self->parsing_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(run_heuristics, std::move(context),
                           ignore_small_forms),
            std::move(update_cache));
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), run_heuristics,
      std::move(update_cache),
      /*ignore_small_forms=*/!client().IsTabInActorMode());

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
        /*ignore_small_forms=*/!manager->client().IsTabInActorMode(),
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

void AutofillManager::PopulateCacheForQueryResponse(
    base::span<const FormData> forms,
    const AutofillCrowdsourcingManager::QueryResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Create a set of the signatures that were returned by the server query.
  base::flat_set<FormSignature> queried_signatures(
      response.queried_form_signatures);

  // Add the forms to the cache if their signature was part of the query
  // and if they do not already exist in the cache. Forms may already exist
  // if local parsing finishes before the server predictions arrived or due
  // to a previous request. Already existing forms are updated if the
  // incoming form has a higher version.
  for (const FormData& form : forms) {
    FormStructure* form_structure =
        FindCachedFormById(form.global_id(), /*pass_key=*/{});
    if (form_structure && form_structure->version() >= form.version()) {
      continue;
    }

    if (!queried_signatures.contains(CalculateFormSignature(form))) {
      continue;
    }

    const bool is_new_form = !form_structure;
    if (form_structures_.size() + is_new_form >
        kAutofillManagerMaxFormCacheSize) {
      LOG_AF(log_manager()) << LoggingScope::kAbortParsing
                            << LogMessage::kAbortParsingTooManyForms << form;
      break;
    }

    // TODO(crbug.com/470949499): This introduces redundancy in
    // UpdateFormCache(), as we would call
    // FormStructure::UpdateFormData(form_data) where form_data and the data
    // in FormStructure are already identical.
    if (is_new_form) {
      form_structures_[form.global_id()] =
          std::make_unique<FormStructure>(form);
    } else {
      form_structure->UpdateFormData(form, /*pass_key=*/{});
    }
  }
}

void AutofillManager::OnLoadedServerPredictions(
    base::span<const FormData> forms,
    base::TimeTicks form_seen_timestamp,
    std::optional<AutofillCrowdsourcingManager::QueryResponse> response) {
  if (!form_seen_timestamp.is_null()) {
    base::UmaHistogramTimes(
        "Autofill.TimingInterval.FormsSeen.LoadedServerPredictions",
        base::TimeTicks::Now() - form_seen_timestamp);
  }
  absl::Cleanup on_after_loaded_server_predictions = [this] {
    NotifyObservers(&Observer::OnAfterLoadedServerPredictions);
  };

  if (!response) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillServerQueryPredictionsEarly)) {
    // Update the form cache with the form structures that were part of the
    // query. The form structures are conditionally created here depending on
    // whether local parsing has already finished and populated the cache.
    PopulateCacheForQueryResponse(forms, *response);
  }

  std::vector<raw_ref<FormStructure>> queried_forms;
  queried_forms.reserve(forms.size());

  if (std::vector<ServerPredictions> form_server_predictions =
          ParseServerPredictionsFromQueryResponse(
              std::move(response->response), forms,
              response->queried_form_signatures, log_manager(),
              /*ignore_small_forms=*/!client().IsTabInActorMode());
      !form_server_predictions.empty()) {
    CHECK_EQ(forms.size(), form_server_predictions.size());
    // TODO(crbug.com/475586865): Use `AutofillManager::UpdateFormCache()`
    // instead of duplicating the logic.
    for (auto [form, server_predictions] :
         base::zip(forms, form_server_predictions)) {
      FormStructure* form_structure =
          FindCachedFormById(form.global_id(), /*pass_key=*/{});
      if (!form_structure) {
        continue;
      }

      queried_forms.emplace_back(*form_structure);
      server_predictions.ApplyTo(*form_structure);
      form_structure->RationalizeAndAssignSections(
          client().GetVariationConfigCountryCode(), GetCurrentPageLanguage(),
          log_manager());
      LogCurrentFieldTypes(form_structure);
      NotifyObservers(&Observer::OnFieldTypesDetermined, form.global_id(),
                      Observer::FieldTypeSource::kAutofillServer,
                      /*small_forms_were_parsed=*/client().IsTabInActorMode());
      if (base::FeatureList::IsEnabled(
              features::kAutofillServerQueryPredictionsEarly)) {
        OnFormProcessed(form, *form_structure);
      }
    }
    LogServerQueryResponseMetrics(queried_forms);
  }

  if (base::FeatureList::IsEnabled(features::debug::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIdsInAllFrames();
  }
  // TODO(crbug.com/470949499): Consider merging OnFormProcessed() and
  // OnLoadedServerPredictionsImpl().
  OnLoadedServerPredictionsImpl(queried_forms);
}

void AutofillManager::LogServerQueryResponseMetrics(
    const std::vector<raw_ref<FormStructure>>& forms) {
  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;
  for (raw_ref<FormStructure> form : forms) {
    for (const std::unique_ptr<AutofillField>& field : form->fields()) {
      FieldType heuristic_type = field->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE) {
        heuristics_detected_fillable_field = true;
      }
      if (!field->Type().GetTypes().contains(heuristic_type)) {
        query_response_overrode_heuristics = true;
      }
    }
    AutofillMetrics::LogServerResponseHasDataForForm(std::ranges::any_of(
        form->fields(), [](FieldType t) { return t != NO_SERVER_DATA; },
        &AutofillField::server_type));
    autofill_metrics::LogQualityMetricsBasedOnAutocomplete(
        *form, client().GetFormInteractionsUkmLogger(),
        driver().GetPageUkmSourceId());
  }

  // TODO(crbug.com/470949499): Clean up these metrics once the
  // `kAutofillServerQueryPredictionsEarly` flag is launched. The metric
  // `QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS` will always be logged if the
  // server finishes first, which is non-deterministic.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillServerQueryPredictionsEarly)) {
    AutofillMetrics::ServerQueryMetric metric;
    if (query_response_overrode_heuristics &&
        heuristics_detected_fillable_field) {
      metric = AutofillMetrics::QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS;
    } else if (query_response_overrode_heuristics) {
      metric = AutofillMetrics::QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS;
    } else {
      metric = AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS;
    }
    AutofillMetrics::LogServerQueryMetric(metric);
  }
}

void AutofillManager::UpdateFormCache(
    base::span<const FormData> forms,
    base::optional_ref<const AsyncContext> context,
    FormStructure::RetrieveFromCacheReason reason,
    bool preserve_signatures) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.UpdateCache");

  auto reset_predictions = [](FormStructure& form_structure) {
    for (const std::unique_ptr<AutofillField>& field :
         form_structure.fields()) {
      // This is set by running field classification heuristics and the ML
      // model.
      for (int i = 0; i <= static_cast<int>(HeuristicSource::kMaxValue); ++i) {
        HeuristicSource s = static_cast<HeuristicSource>(i);
        // Resetting all `HeuristicSource`s also resets the
        // `GetActiveHeuristicSource()`, which in turn resets
        // AutofillField::overall_type_.
        field->set_heuristic_type(s, NO_SERVER_DATA);
      }
      // This is set by running the ML model.
      field->set_ml_supported_types({});
    }
  };

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
    FormStructure* cached_form_structure =
        FindCachedFormById(forms[i].global_id(), /*pass_key=*/{});
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

    if (base::FeatureList::IsEnabled(features::kAutofillOptimizeCacheUpdates)) {
      FormSignature form_signature = cached_form_structure->form_signature();
      FormSignature structural_form_signature =
          cached_form_structure->structural_form_signature();
      cached_form_structure->UpdateFormData(forms[i], /*pass_key=*/{});
      if (context) {
        reset_predictions(*cached_form_structure);
        apply_predictions(*cached_form_structure, *context, i);
      }
      if (preserve_signatures ||
          IsCreditCardFormForSignaturePurposes(
              *cached_form_structure, GetAcUnrecognizedBehavior(client()))) {
        // Not updating signatures of credit card forms is legacy behavior. We
        // believe that the signatures are kept stable for voting purposes.
        // Credit card forms are those which contain only credit card fields.
        // TODO(crbug.com/431754194): Investigate making the behavior consistent
        // across all form types.
        cached_form_structure->set_form_signature(form_signature);
        cached_form_structure->set_structural_form_signature(
            structural_form_signature);
      }
    } else {
      auto form_structure = std::make_unique<FormStructure>(forms[i]);
      form_structure->RetrieveFromCache(*cached_form_structure, reason);
      if (context) {
        apply_predictions(*form_structure, *context, i);
      }

      if (!preserve_signatures &&
          !IsCreditCardFormForSignaturePurposes(
              *cached_form_structure, GetAcUnrecognizedBehavior(client()))) {
        // Not updating signatures of credit card forms is legacy behavior. We
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
