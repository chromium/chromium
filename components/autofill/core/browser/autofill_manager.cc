// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"
#include <functional>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"
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

// Collects the FormGlobalIds of `forms`.
std::vector<FormGlobalId> GetFormGlobalIds(base::span<const FormData> forms) {
  std::vector<FormGlobalId> form_ids;
  form_ids.reserve(forms.size());
  for (const FormData& form : forms) {
    form_ids.push_back(form.global_id());
  }
  return form_ids;
}

// Returns the AutofillField* corresponding to |field| in |form| or nullptr,
// if not found.
AutofillField* FindAutofillFillField(const FormStructure& form,
                                     const FormFieldData& field) {
  for (const auto& f : form) {
    if (field.global_id() == f->global_id())
      return f.get();
  }
  for (const auto& cur_field : form) {
    if (cur_field->SameFieldAs(field)) {
      return cur_field.get();
    }
  }
  return nullptr;
}

// Returns true if |live_form| does not match |cached_form|.
// TODO(crbug.com/1211834): This should be some form of FormData::DeepEqual().
bool CachedFormNeedsUpdate(const FormData& live_form,
                           const FormStructure& cached_form) {
  if (cached_form.version() > live_form.version)
    return false;

  if (live_form.fields.size() != cached_form.field_count())
    return true;

  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    if (!cached_form.field(i)->SameFieldAs(live_form.fields[i]))
      return true;
  }

  return false;
}

}  // namespace

// static
void AutofillManager::LogAutofillTypePredictionsAvailable(
    LogManager* log_manager,
    const std::vector<FormStructure*>& forms) {
  LogBuffer buffer(IsLoggingActive(log_manager));
  for (FormStructure* form : forms)
    LOG_AF(buffer) << *form;

  LOG_AF(log_manager) << LoggingScope::kParsing << LogMessage::kParsedForms
                      << std::move(buffer);
}

AutofillManager::AutofillManager(AutofillDriver* driver, AutofillClient* client)
    : driver_(CHECK_DEREF(driver)),
      client_(CHECK_DEREF(client)),
      log_manager_(client->GetLogManager()),
      form_interactions_ukm_logger_(CreateFormInteractionsUkmLogger()) {
  translate::TranslateDriver* translate_driver = client->GetTranslateDriver();
  if (translate_driver) {
    translate_observation_.Observe(translate_driver);
  }
}

AutofillManager::~AutofillManager() {
  NotifyObservers(&Observer::OnAutofillManagerDestroyed);
  translate_observation_.Reset();
}

// TODO(crbug.com/1309848): Unify form parsing logic.
// TODO(crbug.com/1465926): ML predictions are not computed here since
// `kAutofillPageLanguageDetection` is disabled by default. Once the form
// parsing logic is unified with `ParseFormsAsync()`, this won't be necessary
// anymore.
void AutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (!base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection))
    return;
  if (details.adopted_language == translate::kUnknownLanguageCode ||
      !driver_->IsInActiveFrame()) {
    return;
  }

  NotifyObservers(&Observer::OnBeforeLanguageDetermined);
  LanguageCode lang(details.adopted_language);
  for (auto& [form_id, form_structure] : form_structures_)
    form_structure->set_current_page_language(lang);

  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    for (auto& [form_id, form_structure] : form_structures_) {
      form_structure->DetermineHeuristicTypes(
          client().GetVariationConfigCountryCode(),
          form_interactions_ukm_logger(), log_manager_);
      NotifyObservers(&Observer::OnFieldTypesDetermined, form_id,
                      Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
    }
    NotifyObservers(&Observer::OnAfterLanguageDetermined);
    return;
  }

  struct AsyncContext {
    AsyncContext(
        std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures,
        GeoIpCountryCode country_code,
        LogManager* log_manager)
        : form_structures(std::move(form_structures)),
          country_code(std::move(country_code)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures;
    GeoIpCountryCode country_code;
    std::unique_ptr<BufferingLogManager> log_manager;
  };

  // To be run on a different task (must not access global or member
  // variables).
  // TODO(crbug.com/1309848): We can't pass a UKM logger because it's a member
  // variable. To be fixed.
  auto RunHeuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Autofill.Timing.OnLanguageDetermined.RunHeuristics");
    for (auto& [id, form_structure] : context.form_structures) {
      form_structure->DetermineHeuristicTypes(
          context.country_code,
          /*form_interactions_ukm_logger=*/nullptr, context.log_manager.get());
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto UpdateCache = [](base::WeakPtr<AutofillManager> self,
                        AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Autofill.Timing.OnLanguageDetermined.UpdateCache");
    if (!self) {
      return;
    }
    if (context.log_manager && self->log_manager_) {
      context.log_manager->Flush(*self->log_manager_);
    }
    for (auto& [id, form_structure] : context.form_structures) {
      self->form_structures_[id] = std::move(form_structure);
      self->NotifyObservers(
          &Observer::OnFieldTypesDetermined, id,
          Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
    }
    self->NotifyObservers(&Observer::OnAfterLanguageDetermined);
  };

  // Transfers ownership of the cached `form_structures_` to the worker task,
  // which will eventually move them back into `form_structures_`. This means
  // `AutofillManager::form_structures_` is empty for a brief period of time.
  auto form_structures = std::exchange(form_structures_, {});
  parsing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          RunHeuristics,
          AsyncContext(std::move(form_structures),
                       client().GetVariationConfigCountryCode(), log_manager_)),
      base::BindOnce(UpdateCache, parsing_weak_ptr_factory_.GetWeakPtr()));
}

void AutofillManager::OnTranslateDriverDestroyed(
    translate::TranslateDriver* translate_driver) {
  translate_observation_.Reset();
}

LanguageCode AutofillManager::GetCurrentPageLanguage() {
  const translate::LanguageState* language_state = client().GetLanguageState();
  if (!language_state)
    return LanguageCode();
  return LanguageCode(language_state->current_language());
}

void AutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  if (!IsValidFormData(form))
    return;

  NotifyObservers(&Observer::OnBeforeDidFillAutofillFormData, form.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnDidFillAutofillFormDataImpl(form, timestamp);
    NotifyObservers(&Observer::OnAfterDidFillAutofillFormData,
                    form.global_id());
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnDidFillAutofillFormDataImpl,
                      timestamp)
          .Then(NotifyObserversCallback(
              &Observer::OnAfterDidFillAutofillFormData, form.global_id())));
}

void AutofillManager::OnFormSubmitted(const FormData& form,
                                      const bool known_success,
                                      const mojom::SubmissionSource source) {
  if (!IsValidFormData(form)) {
    return;
  }

  NotifyObservers(&Observer::OnFormSubmitted, form.global_id());
  OnFormSubmittedImpl(form, known_success, source);
}

void AutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  // Erase forms that have been removed from the DOM. This prevents
  // |form_structures_| from growing up its upper bound
  // kAutofillManagerMaxFormCacheSize.
  for (FormGlobalId removed_form : removed_forms)
    form_structures_.erase(removed_form);

  if (!IsValidFormDataVector(updated_forms)) {
    return;
  }

  if (!ShouldParseForms()) {
    return;
  }

  NotifyObservers(&Observer::OnBeforeFormsSeen,
                  GetFormGlobalIds(updated_forms));
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    std::vector<FormData> parsed_forms;
    for (const FormData& form : updated_forms) {
      const auto parse_form_start_time = AutofillTickClock::NowTicks();
      FormStructure* cached_form_structure =
          FindCachedFormById(form.global_id());

      // Not updating signatures of credit card forms is legacy behaviour. We
      // believe that the signatures are kept stable for voting purposes.
      bool update_form_signature = false;
      if (cached_form_structure) {
        const DenseSet<FormType>& form_types =
            cached_form_structure->GetFormTypes();
        update_form_signature =
            form_types.size() > form_types.count(FormType::kCreditCardForm);
      }

      FormStructure* form_structure = ParseForm(form, cached_form_structure);
      if (!form_structure)
        continue;
      DCHECK(form_structure);

      if (update_form_signature) {
        form_structure->set_form_signature(CalculateFormSignature(form));
        form_structure->set_alternative_form_signature(
            CalculateAlternativeFormSignature(form));
      }

      parsed_forms.push_back(form);
      AutofillMetrics::LogParseFormTiming(AutofillTickClock::NowTicks() -
                                          parse_form_start_time);
    }
    if (!parsed_forms.empty())
      OnFormsParsed(parsed_forms);
    NotifyObservers(&Observer::OnAfterFormsSeen,
                    GetFormGlobalIds(parsed_forms));
    return;
  }
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillParseAsync));
  auto ProcessParsedForms = [](AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty())
      self.OnFormsParsed(parsed_forms);
    self.NotifyObservers(&Observer::OnAfterFormsSeen,
                         GetFormGlobalIds(parsed_forms));
  };
  ParseFormsAsync(updated_forms, base::BindOnce(ProcessParsedForms));
}

void AutofillManager::OnFormsParsed(const std::vector<FormData>& forms) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  driver().HandleParsedForms(forms);

  std::vector<FormStructure*> non_queryable_forms;
  std::vector<FormStructure*> queryable_forms;
  DenseSet<FormType> form_types;
  for (const FormData& form : forms) {
    FormStructure* form_structure = FindCachedFormById(form.global_id());
    if (!form_structure) {
      NOTREACHED();
      continue;
    }

    form_types.insert_all(form_structure->GetFormTypes());

    // Configure the query encoding for this form and add it to the appropriate
    // collection of forms: queryable vs non-queryable.
    if (form_structure->ShouldBeQueried()) {
      queryable_forms.push_back(form_structure);
    } else {
      non_queryable_forms.push_back(form_structure);
    }

    OnFormProcessed(form, *form_structure);
  }

  if (!queryable_forms.empty() || !non_queryable_forms.empty()) {
    OnAfterProcessParsedForms(form_types);
  }

  // Send the current type predictions to the renderer. For non-queryable forms
  // this is all the information about them that will ever be available. The
  // queryable forms will be updated once the field type query is complete.
  driver().SendAutofillTypePredictionsToRenderer(non_queryable_forms);
  driver().SendAutofillTypePredictionsToRenderer(queryable_forms);
  // Send the fields that are eligible for manual filling to the renderer. If
  // server predictions are not yet available for these forms, the eligible
  // fields would be updated again once they are available.
  driver().SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(non_queryable_forms));
  driver().SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(queryable_forms));
  LogAutofillTypePredictionsAvailable(log_manager_, non_queryable_forms);
  LogAutofillTypePredictionsAvailable(log_manager_, queryable_forms);

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty() && client().GetCrowdsourcingManager()) {
    NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
    if (!client().GetCrowdsourcingManager()->StartQueryRequest(
            queryable_forms, driver().IsolationInfo(), GetWeakPtr())) {
      NotifyObservers(&Observer::OnAfterLoadedServerPredictions);
    }
  }
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           const base::TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeTextFieldDidChange, form.global_id(),
                  field.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnTextFieldDidChangeImpl(form, field, bounding_box, timestamp);
    NotifyObservers(&Observer::OnAfterTextFieldDidChange, form.global_id(),
                    field.global_id(), field.value);
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnTextFieldDidChangeImpl, field,
                            bounding_box, timestamp)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterTextFieldDidChange, form.global_id(),
                    field.global_id(), field.value)));
}

void AutofillManager::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeTextFieldDidScroll, form.global_id(),
                  field.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnTextFieldDidScrollImpl(form, field, bounding_box);
    NotifyObservers(&Observer::OnAfterTextFieldDidScroll, form.global_id(),
                    field.global_id());
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnTextFieldDidScrollImpl, field,
                      bounding_box)
          .Then(NotifyObserversCallback(&Observer::OnAfterTextFieldDidScroll,
                                        form.global_id(), field.global_id())));
}

void AutofillManager::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeSelectControlDidChange, form.global_id(),
                  field.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnSelectControlDidChangeImpl(form, field, bounding_box);
    NotifyObservers(&Observer::OnAfterSelectControlDidChange, form.global_id(),
                    field.global_id());
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnSelectControlDidChangeImpl,
                            field, bounding_box)
                .Then(NotifyObserversCallback(
                    &Observer::OnAfterSelectControlDidChange, form.global_id(),
                    field.global_id())));
}

void AutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeAskForValuesToFill, form.global_id(),
                  field.global_id(), form);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnAskForValuesToFillImpl(form, field, bounding_box, trigger_source);
    NotifyObservers(&Observer::OnAfterAskForValuesToFill, form.global_id(),
                    field.global_id());
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnAskForValuesToFillImpl, field,
                      bounding_box, trigger_source)
          .Then(NotifyObserversCallback(&Observer::OnAfterAskForValuesToFill,
                                        form.global_id(), field.global_id())));
}

void AutofillManager::OnFocusOnFormField(const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnFocusOnFormFieldImpl(form, field, bounding_box);
    return;
  }
  ParseFormAsync(form, ParsingCallback(&AutofillManager::OnFocusOnFormFieldImpl,
                                       field, bounding_box)
                           .Then(NotifyNoObserversCallback()));
}

void AutofillManager::OnFocusNoLongerOnForm(bool had_interacted_form) {
  OnFocusNoLongerOnFormImpl(had_interacted_form);
}

void AutofillManager::OnDidEndTextFieldEditing() {
  OnDidEndTextFieldEditingImpl();
}

void AutofillManager::OnHidePopup() {
  OnHidePopupImpl();
}

void AutofillManager::OnPopupHidden() {
  driver().PopupHidden();
  NotifyObservers(&Observer::OnSuggestionsHidden);
}

void AutofillManager::OnSelectOrSelectListFieldOptionsDidChange(
    const FormData& form) {
  if (!IsValidFormData(form))
    return;

  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnSelectOrSelectListFieldOptionsDidChangeImpl(form);
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(
                &AutofillManager::OnSelectOrSelectListFieldOptionsDidChangeImpl)
                .Then(NotifyNoObserversCallback()));
}

void AutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value) {
  if (!IsValidFormData(form))
    return;

  NotifyObservers(&Observer::OnBeforeJavaScriptChangedAutofilledValue,
                  form.global_id(), field.global_id());
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnJavaScriptChangedAutofilledValueImpl(form, field, old_value);
    NotifyObservers(&Observer::OnAfterJavaScriptChangedAutofilledValue,
                    form.global_id(), field.global_id());
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnJavaScriptChangedAutofilledValueImpl,
                      field, old_value)
          .Then(NotifyObserversCallback(
              &Observer::OnAfterJavaScriptChangedAutofilledValue,
              form.global_id(), field.global_id())));
}

// Returns true if |live_form| does not match |cached_form|.
bool AutofillManager::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Maybe find an existing FormStructure that corresponds to |form|.
  FormStructure* cached_form = FindCachedFormById(form.global_id());
  if (cached_form) {
    if (base::FeatureList::IsEnabled(features::kAutofillParseAsync) ||
        !CachedFormNeedsUpdate(form, *cached_form)) {
      // There is no data to return if there are no auto-fillable fields.
      if (!cached_form->autofill_count())
        return false;

      // Return the cached form and matching field, if any.
      *form_structure = cached_form;
      *autofill_field = FindAutofillFillField(**form_structure, field);
      return *autofill_field != nullptr;
    }
  }

  if (base::FeatureList::IsEnabled(features::kAutofillParseAsync))
    return false;

  // The form is new or updated, parse it and discard |cached_form|.
  // i.e., |cached_form| is no longer valid after this call.
  *form_structure = ParseForm(form, cached_form);
  if (!*form_structure)
    return false;

  // Annotate the updated form with its predicted types.
  driver().SendAutofillTypePredictionsToRenderer({*form_structure});
  // Update the renderer with the latest set of fields eligible for manual
  // filling.
  driver().SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling({*form_structure}));
  // There is no data to return if there are no auto-fillable fields.
  if (!(*form_structure)->autofill_count())
    return false;

  // Find the AutofillField that corresponds to |field|.
  *autofill_field = FindAutofillFillField(**form_structure, field);
  return *autofill_field != nullptr;
}

std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
AutofillManager::CreateFormInteractionsUkmLogger() {
  return std::make_unique<AutofillMetrics::FormInteractionsUkmLogger>(
      &unsafe_client(), unsafe_client().GetUkmRecorder());
}

size_t AutofillManager::FindCachedFormsBySignature(
    FormSignature form_signature,
    std::vector<FormStructure*>* form_structures) const {
  size_t hits_num = 0;
  for (const auto& [form_id, form_structure] : form_structures_) {
    if (form_structure->form_signature() == form_signature) {
      ++hits_num;
      if (form_structures)
        form_structures->push_back(form_structure.get());
    }
  }
  return hits_num;
}

FormStructure* AutofillManager::FindCachedFormById(FormGlobalId form_id) const {
  auto it = form_structures_.find(form_id);
  return it != form_structures_.end() ? it->second.get() : nullptr;
}

bool AutofillManager::CanShowAutofillUi() const {
  return driver_->CanShowAutofillUi();
}

void AutofillManager::TriggerFormExtractionInAllFrames(
    base::OnceCallback<void(bool success)> form_extraction_finished_callback) {
  driver_->TriggerFormExtractionInAllFrames(
      std::move(form_extraction_finished_callback));
}

void AutofillManager::ParseFormsAsync(
    const std::vector<FormData>& forms,
    base::OnceCallback<void(AutofillManager&, const std::vector<FormData>&)>
        callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync");
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillParseAsync));

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
      LOG_AF(log_manager_) << LoggingScope::kAbortParsing
                           << LogMessage::kAbortParsingTooManyForms
                           << form_data;
      continue;
    }

    auto form_structure = std::make_unique<FormStructure>(form_data);
    if (!form_structure->ShouldBeParsed(log_manager_))
      continue;

    num_managed_forms += is_new_form;
    DCHECK_LE(num_managed_forms, kAutofillManagerMaxFormCacheSize);

    if (FormStructure* cached_form_structure =
            FindCachedFormById(form_data.global_id())) {
      // We need to keep the server data if available. We need to use them while
      // determining the heuristics.
      form_structure->RetrieveFromCache(
          *cached_form_structure,
          FormStructure::RetrieveFromCacheReason::kFormParsing);

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
  base::ranges::sort(form_structures, {}, &FormStructure::global_id);
  form_structures.erase(
      base::ranges::unique(form_structures, {}, &FormStructure::global_id),
      form_structures.end());

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
  // TODO(crbug.com/1309848): We can't pass a UKM logger because it's a member
  // variable. To be fixed.
  auto run_heuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.RunHeuristics");
    for (auto& form_structure : context.form_structures) {
      form_structure->DetermineHeuristicTypes(
          context.country_code,
          /*form_interactions_ukm_logger=*/nullptr, context.log_manager.get());
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(AutofillManager&,
                                 const std::vector<FormData>&)> callback,
         const std::vector<FormData>& parsed_forms, AsyncContext context) {
        SCOPED_UMA_HISTOGRAM_TIMER(
            "Autofill.Timing.ParseFormsAsync.UpdateCache");
        if (!self) {
          return;
        }
        if (context.log_manager && self->log_manager_) {
          context.log_manager->Flush(*self->log_manager_);
        }
        for (auto& form_structure : context.form_structures) {
          FormGlobalId id = form_structure->global_id();
          self->form_structures_[id] = std::move(form_structure);
          self->NotifyObservers(
              &Observer::OnFieldTypesDetermined, id,
              Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
        }
        std::move(callback).Run(*self, parsed_forms);
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      parsed_forms);

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
                             self->log_manager_)),
            std::move(update_cache));
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), run_heuristics,
      std::move(update_cache));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Run ML Model before running heuristics to ensure that
  // rationalization and sectioning are done.
  if (auto* ml_handler = client().GetAutofillMlPredictionModelHandler()) {
    ml_handler->GetModelPredictionsForForms(
        std::move(form_structures), std::move(run_heuristics_and_update_cache));
    return;
  }
#endif
  std::move(run_heuristics_and_update_cache).Run(std::move(form_structures));
}

void AutofillManager::ParseFormAsync(
    const FormData& form_data,
    base::OnceCallback<void(AutofillManager&, const FormData&)> callback) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormAsync");
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillParseAsync));

  bool is_new_form = !base::Contains(form_structures_, form_data.global_id());
  if (form_structures_.size() + is_new_form >
      kAutofillManagerMaxFormCacheSize) {
    LOG_AF(log_manager_) << LoggingScope::kAbortParsing
                         << LogMessage::kAbortParsingTooManyForms << form_data;
    return;
  }

  auto form_structure = std::make_unique<FormStructure>(form_data);
  if (!form_structure->ShouldBeParsed(log_manager_)) {
    // For Autocomplete, events need to be handled even for forms that cannot be
    // parsed.
    std::move(callback).Run(*this, form_data);
    return;
  }

  if (FormStructure* cached_form_structure =
          FindCachedFormById(form_data.global_id())) {
    if (!CachedFormNeedsUpdate(form_data, *cached_form_structure)) {
      std::move(callback).Run(*this, form_data);
      return;
    }

    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(
        *cached_form_structure,
        FormStructure::RetrieveFromCacheReason::kFormParsing);
  }
  form_structure->set_current_page_language(GetCurrentPageLanguage());

  struct AsyncContext {
    AsyncContext(std::unique_ptr<FormStructure> form_structure,
                 GeoIpCountryCode country_code,
                 LogManager* log_manager)
        : form_structure(std::move(form_structure)),
          country_code(std::move(country_code)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::unique_ptr<FormStructure> form_structure;
    GeoIpCountryCode country_code;
    std::unique_ptr<BufferingLogManager> log_manager;
  };

  // To be run on a different task (must not access global or member
  // variables).
  // TODO(crbug.com/1309848): We can't pass a UKM logger because it's a member
  // variable. To be fixed.
  auto run_heuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormAsync.RunHeuristics");
    context.form_structure->DetermineHeuristicTypes(
        context.country_code,
        /*form_interactions_ukm_logger=*/nullptr, context.log_manager.get());
    return context;
  };

  // To be run on the main thread (accesses member variables).
  // The reason this takes both `form_data` and `form_structure` is that they
  // may disagree on the form's values: if the form is seen for the second time,
  // RetrieveFromCache() resets the `form_structure`'s fields.
  // TODO(crbug/1345089): Make FormStructure's and FormData's fields correspond,
  // migrate all event handlers in BrowserAutofillManager take a FormStructure,
  // and drop the FormData from UpdateCache().
  auto update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(AutofillManager&, const FormData&)> callback,
         const FormData& form_data, AsyncContext context) {
        SCOPED_UMA_HISTOGRAM_TIMER(
            "Autofill.Timing.ParseFormAsync.UpdateCache");
        if (!self) {
          return;
        }
        if (context.log_manager && self->log_manager_) {
          context.log_manager->Flush(*self->log_manager_);
        }
        FormGlobalId id = context.form_structure->global_id();
        self->form_structures_[id] = std::move(context.form_structure);
        self->NotifyObservers(
            &Observer::OnFieldTypesDetermined, id,
            Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
        std::move(callback).Run(*self, form_data);
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), std::move(callback), form_data);

  // To be run on the main thread (accesses member variables).
  auto run_heuristics_and_update_cache = base::BindOnce(
      [](base::WeakPtr<AutofillManager> self,
         AsyncContext (*run_heuristics)(AsyncContext),
         base::OnceCallback<void(AsyncContext)> update_cache,
         std::unique_ptr<FormStructure> form) {
        if (!self) {
          return;
        }
        self->parsing_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                run_heuristics,
                AsyncContext(std::move(form),
                             self->client().GetVariationConfigCountryCode(),
                             self->log_manager_)),
            std::move(update_cache));
      },
      parsing_weak_ptr_factory_.GetWeakPtr(), run_heuristics,
      std::move(update_cache));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Run ML Model before running heuristics to ensure that
  // rationalization and sectioning are done.
  if (auto* ml_handler = client().GetAutofillMlPredictionModelHandler()) {
    ml_handler->GetModelPredictionsForForm(
        std::move(form_structure), std::move(run_heuristics_and_update_cache));
    return;
  }
#endif
  std::move(run_heuristics_and_update_cache).Run(std::move(form_structure));
}

FormStructure* AutofillManager::ParseForm(const FormData& form,
                                          const FormStructure* cached_form) {
  DCHECK(!base::FeatureList::IsEnabled(features::kAutofillParseAsync));

  if (form_structures_.size() >= kAutofillManagerMaxFormCacheSize) {
    LOG_AF(log_manager_) << LoggingScope::kAbortParsing
                         << LogMessage::kAbortParsingTooManyForms << form;
    return nullptr;
  }

  auto form_structure = std::make_unique<FormStructure>(form);
  if (!form_structure->ShouldBeParsed(log_manager_))
    return nullptr;

  if (cached_form) {
    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(
        *cached_form, FormStructure::RetrieveFromCacheReason::kFormParsing);
  }

  form_structure->set_current_page_language(GetCurrentPageLanguage());

  form_structure->DetermineHeuristicTypes(
      client().GetVariationConfigCountryCode(), form_interactions_ukm_logger(),
      log_manager_);

  // Hold the parsed_form_structure we intend to return. We can use this to
  // reference the form_signature when transferring ownership below.
  FormStructure* parsed_form_structure = form_structure.get();

  // Ownership is transferred to |form_structures_| which maintains it until
  // the form is parsed again or the AutofillManager is destroyed.
  //
  // Note that this insert/update takes ownership of the new form structure
  // and also destroys the previously cached form structure.
  form_structures_[parsed_form_structure->global_id()] =
      std::move(form_structure);

  NotifyObservers(&Observer::OnFieldTypesDetermined,
                  parsed_form_structure->global_id(),
                  Observer::FieldTypeSource::kHeuristicsOrAutocomplete);
  return parsed_form_structure;
}

void AutofillManager::Reset() {
  parsing_weak_ptr_factory_.InvalidateWeakPtrs();
  NotifyObservers(&Observer::OnAutofillManagerReset);
  form_structures_.clear();
  form_interactions_ukm_logger_ = CreateFormInteractionsUkmLogger();
}

void AutofillManager::OnLoadedServerPredictions(
    std::string response,
    const std::vector<FormSignature>& queried_form_signatures) {
  // Get the current valid FormStructures represented by
  // |queried_form_signatures|.
  std::vector<FormStructure*> queried_forms;
  queried_forms.reserve(queried_form_signatures.size());
  for (const auto& form_signature : queried_form_signatures) {
    FindCachedFormsBySignature(form_signature, &queried_forms);
  }

  // Each form signature in |queried_form_signatures| is supposed to be unique,
  // and therefore appear only once. This ensures that
  // FindCachedFormsBySignature() produces an output without duplicates in the
  // forms.
  // TODO(crbug/1064709): |queried_forms| could be a set data structure; their
  // order should be irrelevant.
  DCHECK_EQ(queried_forms.size(),
            std::set<FormStructure*>(queried_forms.begin(), queried_forms.end())
                .size());

  // If there are no current forms corresponding to the queried signatures, drop
  // the query response.
  if (queried_forms.empty()) {
    NotifyObservers(&Observer::OnAfterLoadedServerPredictions);
    return;
  }

  // Parse and store the server predictions.
  FormStructure::ParseApiQueryResponse(
      std::move(response), queried_forms, queried_form_signatures,
      form_interactions_ukm_logger(), log_manager_);

  // Will log quality metrics for each FormStructure based on the presence of
  // autocomplete attributes, if available.
  if (auto* logger = form_interactions_ukm_logger()) {
    for (FormStructure* cur_form : queried_forms) {
      autofill_metrics::LogQualityMetricsBasedOnAutocomplete(*cur_form, logger);
    }
  }

  // Send field type predictions to the renderer so that it can possibly
  // annotate forms with the predicted types or add console warnings.
  driver().SendAutofillTypePredictionsToRenderer(queried_forms);

  driver().SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(queried_forms));

  LogAutofillTypePredictionsAvailable(log_manager_, queried_forms);

  for (const FormStructure* form : queried_forms) {
    NotifyObservers(&Observer::OnFieldTypesDetermined, form->global_id(),
                    Observer::FieldTypeSource::kAutofillServer);
  }

  NotifyObservers(&Observer::OnAfterLoadedServerPredictions);
}

}  // namespace autofill
