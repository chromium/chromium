// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/autofill_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_ref.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/language_detection/core/constants.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/translate/core/common/language_detection_details.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#endif

namespace autofill {

namespace {

// ParsingCallback(), NotifyObserversCallback(), and NotifyNoObserversCallback()
// assemble the reply callback for ParseFormAsync().
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
//       .Then(NotifyNoObserversCallback())
// or
//   ParsingCallback(&AutofillManager::OnFooImpl, ...)
//       .Then(NotifyObserversCallback(&Observer::OnAfterFoo, ...))
//
// `.Then(NotifyNoObserversCallback())` is needed in the first case to discard
// the return type of ParsingCallback().
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

// See ParsingCallback().
base::OnceCallback<void(AutofillManager&)> NotifyNoObserversCallback() {
  return base::DoNothingAs<void(AutofillManager&)>();
}

// Returns true if |live_form| does not match |cached_form|.
// TODO(crbug.com/40183094): This should be some form of FormData::DeepEqual().
bool CachedFormNeedsUpdate(const FormData& live_form,
                           const FormStructure& cached_form) {
  if (cached_form.version() > live_form.version()) {
    return false;
  }

  if (live_form.fields().size() != cached_form.field_count()) {
    return true;
  }

  for (auto [cached_field, live_field] :
       base::zip(cached_form.fields(), live_form.fields())) {
    if (!cached_field->SameFieldAs(live_field)) {
      return true;
    }
  }

  return false;
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Retrieves the ML model handler form the `client` using `get_handler`, and
// requests ML predictions for `forms` if the handler is available. Passes
// `callback` to the handler so it is invoked once predictions are available.
void GetMlPredictionsIfNeeded(
    base::WeakPtr<AutofillClient> client,
    FieldClassificationModelHandler* (AutofillClient::*get_handler)(),
    base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
        callback,
    std::vector<std::unique_ptr<FormStructure>> forms) {
  if (!client) {
    return;
  }
  if (FieldClassificationModelHandler* ml_handler = (*client.*get_handler)()) {
    ml_handler->GetModelPredictionsForForms(std::move(forms),
                                            std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms));
  }
}
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace

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
  AfterParsingFinishes(base::BindOnce([](base::WeakPtr<AutofillManager> self) {
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

void AutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeDidFillAutofillFormData, form.global_id());
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnDidFillAutofillFormDataImpl,
                      timestamp)
          .Then(NotifyObserversCallback(
              &Observer::OnAfterDidFillAutofillFormData, form.global_id())));
}

void AutofillManager::OnFormSubmitted(const FormData& form,
                                      const mojom::SubmissionSource source) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnFormSubmitted, form);
  OnFormSubmittedImpl(form, source);
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
  DenseSet<FormType> form_types;
  for (const FormData& form : forms) {
    const FormStructure& form_structure =
        CHECK_DEREF(FindCachedFormById(form.global_id()));

    form_types.insert_all(form_structure.GetFormTypes());

    // Configure the query encoding for this form and add it to the appropriate
    // collection of forms: queryable vs non-queryable.
    if (form_structure.ShouldBeQueried()) {
      queryable_forms.push_back(&form_structure);
    }

    OnFormProcessed(form, form_structure);
  }

  if (base::FeatureList::IsEnabled(features::test::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIDs();
  }

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty()) {
    NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
    // If language detection is currently reparsing the form, wait until the
    // server response is processed, to ensure server predictions are not lost.
    client().GetCrowdsourcingManager().StartQueryRequest(
        queryable_forms, driver().GetIsolationInfo(),
        AfterParsingFinishes(base::BindOnce(
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
    base::optional_ref<const PasswordSuggestionRequest> password_request) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeAskForValuesToFill, form.global_id(),
                  field_id, form);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnAskForValuesToFillImpl, field_id,
                      caret_bounds, trigger_source, password_request)
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
  OnFocusOnNonFormFieldImpl();
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

void AutofillManager::OnSelectFieldOptionsDidChange(const FormData& form) {
  if (!IsValidFormData(form)) {
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnSelectFieldOptionsDidChangeImpl)
                .Then(NotifyNoObserversCallback()));
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
  auto field_it =
      std::ranges::find(*cached_form, field_id, &AutofillField::global_id);
  *autofill_field = field_it == cached_form->end() ? nullptr : field_it->get();
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

base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>
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
AutofillManager::GetHeursticPredictionForForm(
    HeuristicSource source,
    FormGlobalId form_id,
    const std::vector<FieldGlobalId>& field_ids) const {
  FormStructure* cached_form = FindCachedFormById(form_id);
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
  std::vector<FormData> parsed_forms;
  std::vector<std::unique_ptr<FormStructure>> form_structures;
  for (const FormData& form_data : forms) {
    bool is_new_form = !base::Contains(form_structures_, form_data.global_id());
    if (num_managed_forms + is_new_form > kAutofillManagerMaxFormCacheSize) {
      LOG_AF(log_manager())
          << LoggingScope::kAbortParsing
          << LogMessage::kAbortParsingTooManyForms << form_data;
      continue;
    }

    auto form_structure = std::make_unique<FormStructure>(form_data);
    if (!form_structure->ShouldBeParsed(log_manager())) {
      LogCurrentFieldTypes(*form_structure);
      continue;
    }

    num_managed_forms += is_new_form;
    DCHECK_LE(num_managed_forms, kAutofillManagerMaxFormCacheSize);

    if (FormStructure* cached_form_structure =
            FindCachedFormById(form_data.global_id())) {
      // We need to keep the server data if available. We need to use them while
      // determining the heuristics.
      form_structure->RetrieveFromCache(
          *cached_form_structure,
          FormStructure::RetrieveFromCacheReason::kFormCacheUpdateAfterParsing);

      // Not updating signatures of credit card forms is legacy behaviour. We
      // believe that the signatures are kept stable for voting purposes.
      DenseSet<FormType> form_types = cached_form_structure->GetFormTypes();
      if (form_types.size() > form_types.count(FormType::kCreditCardForm)) {
        form_structure->set_form_signature(CalculateFormSignature(form_data));
        form_structure->set_alternative_form_signature(
            CalculateAlternativeFormSignature(form_data));
      }
    }

    form_structure->set_current_page_language(GetCurrentPageLanguage());
    form_structures.push_back(std::move(form_structure));
    parsed_forms.push_back(form_data);
  }

  // Remove duplicates by their FormGlobalId. Otherwise, after moving the forms
  // into `form_structures_`, duplicates may be destroyed and we'd end up with
  // dangling pointers.
  std::ranges::sort(form_structures, {}, &FormStructure::global_id);
  auto repeated =
      std::ranges::unique(form_structures, {}, &FormStructure::global_id);
  form_structures.erase(repeated.begin(), repeated.end());

  ParseFormsAsyncCommon(
      std::move(form_structures),
      base::BindOnce(
          [](base::OnceCallback<void(AutofillManager&,
                                     const std::vector<FormData>&)> callback,
             std::vector<FormData> form_datas, AutofillManager& manager) {
            std::move(callback).Run(manager, form_datas);
          },
          std::move(callback), std::move(parsed_forms)));
}

void AutofillManager::ParseFormAsync(
    const FormData& form_data,
    base::OnceCallback<void(AutofillManager&, const FormData&)> callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormAsync");

  bool is_new_form = !base::Contains(form_structures_, form_data.global_id());
  if (form_structures_.size() + is_new_form >
      kAutofillManagerMaxFormCacheSize) {
    LOG_AF(log_manager()) << LoggingScope::kAbortParsing
                          << LogMessage::kAbortParsingTooManyForms << form_data;
    return;
  }

  auto form_structure = std::make_unique<FormStructure>(form_data);
  if (!form_structure->ShouldBeParsed(log_manager())) {
    LogCurrentFieldTypes(*form_structure);
    // For Autocomplete, events need to be handled even for forms that cannot be
    // parsed.
    std::move(callback).Run(*this, form_data);
    return;
  }

  if (FormStructure* cached_form_structure =
          FindCachedFormById(form_data.global_id())) {
    if (!CachedFormNeedsUpdate(form_data, *cached_form_structure)) {
      // Update the cache to the latest data from the renderer in the form
      // cache (in particular, the current field values) while preserving all
      // other information (in particular, the field types).
      form_structure->RetrieveFromCache(*cached_form_structure,
                                        FormStructure::RetrieveFromCacheReason::
                                            kFormCacheUpdateWithoutParsing);
      form_structures_[form_data.global_id()] = std::move(form_structure);
      std::move(callback).Run(*this, form_data);
      return;
    }

    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(
        *cached_form_structure,
        FormStructure::RetrieveFromCacheReason::kFormCacheUpdateAfterParsing);
  }
  form_structure->set_current_page_language(GetCurrentPageLanguage());

  std::vector<std::unique_ptr<FormStructure>> form_structures;
  form_structures.push_back(std::move(form_structure));
  ParseFormsAsyncCommon(
      std::move(form_structures),
      base::BindOnce(
          [](base::OnceCallback<void(AutofillManager&, const FormData&)>
                 callback,
             FormData form_data, AutofillManager& manager) {
            std::move(callback).Run(manager, form_data);
          },
          std::move(callback), std::move(form_data)));
}

void AutofillManager::ParseFormsAsyncCommon(
    std::vector<std::unique_ptr<FormStructure>> form_structures,
    base::OnceCallback<void(AutofillManager&)> callback) {
  struct AsyncContext {
    AsyncContext(std::vector<std::unique_ptr<FormStructure>> form_structures,
                 GeoIpCountryCode country_code,
                 LogManager* log_manager)
        : form_structures(std::move(form_structures)),
          country_code(std::move(country_code)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::vector<std::unique_ptr<FormStructure>> form_structures;
    GeoIpCountryCode country_code;
    std::unique_ptr<BufferingLogManager> log_manager;
  };

  // To be run on a different task (must not access global or member
  // variables).
  auto run_heuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.RunHeuristics");
    for (auto& form_structure : context.form_structures) {
      form_structure->DetermineHeuristicTypes(context.country_code,
                                              context.log_manager.get());
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(AutofillManager&)> callback,
         AsyncContext context) {
        SCOPED_UMA_HISTOGRAM_TIMER(
            "Autofill.Timing.ParseFormsAsync.UpdateCache");
        if (!self) {
          return;
        }
        if (context.log_manager && self->log_manager()) {
          context.log_manager->Flush(*self->log_manager());
        }
        for (auto& form_structure : context.form_structures) {
          FormStructure* raw_form_structure = form_structure.get();
          self->form_structures_[raw_form_structure->global_id()] =
              std::move(form_structure);
          self->LogCurrentFieldTypes(*raw_form_structure);
          self->NotifyObservers(
              &Observer::OnFieldTypesDetermined,
              raw_form_structure->global_id(),
              Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
        }
        std::move(callback).Run(*self);
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  // To be run on the main thread (accesses member variables).
  auto run_heuristics_and_update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         AsyncContext (*run_heuristics)(AsyncContext),
         base::OnceCallback<void(AsyncContext)> update_cache,
         std::vector<std::unique_ptr<FormStructure>> forms) {
        if (!self) {
          return;
        }
        self->parsing_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                run_heuristics,
                AsyncContext(std::move(forms),
                             self->client().GetVariationConfigCountryCode(),
                             self->log_manager())),
            std::move(update_cache));
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), run_heuristics,
      std::move(update_cache));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Parsing happens in the following order:
  // (1) Running ML Models (first Autofill, then Password Manager).
  // (2) Running heuristics (this ensures that rationalization and sectioning
  // are done for the active Autofill predictions).
  // (3) Updating the form cache.

  // Chain running heuristics and updating cache after running the Password
  // Manager model.
  auto run_password_manager_model_if_needed = base::BindOnce(
      &GetMlPredictionsIfNeeded, client().GetWeakPtr(),
      &AutofillClient::GetPasswordManagerFieldClassificationModelHandler,
      std::move(run_heuristics_and_update_cache));

  // Chain running the Password Manager model after running the Autofill model.
  GetMlPredictionsIfNeeded(
      client().GetWeakPtr(),
      &AutofillClient::GetAutofillFieldClassificationModelHandler,
      std::move(run_password_manager_model_if_needed),
      std::move(form_structures));
#else
  std::move(run_heuristics_and_update_cache).Run(std::move(form_structures));
#endif
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
  if (base::FeatureList::IsEnabled(features::test::kShowDomNodeIDs)) {
    driver().ExposeDomNodeIDs();
  }

  for (const raw_ptr<FormStructure, VectorExperimental> form : queried_forms) {
    form->RationalizeAndAssignSections(log_manager(), /*legacy_order=*/true);

    autofill_metrics::LogQualityMetricsBasedOnAutocomplete(
        *form, client().GetFormInteractionsUkmLogger(),
        driver().GetPageUkmSourceId());
    LogCurrentFieldTypes(*form);

    NotifyObservers(&Observer::OnFieldTypesDetermined, form->global_id(),
                    Observer::FieldTypeSource::kAutofillServer);
  }
}

void AutofillManager::LogCurrentFieldTypes(const FormStructure& form) {
  LogBuffer buffer(IsLoggingActive(log_manager()));
  LOG_AF(buffer) << form;
  LOG_AF(log_manager()) << LoggingScope::kParsing << LogMessage::kParsedForms
                        << std::move(buffer);
  if (base::FeatureList::IsEnabled(
          features::test::kAutofillShowTypePredictions)) {
    driver().SendTypePredictionsToRenderer(form);
  }
}

}  // namespace autofill
