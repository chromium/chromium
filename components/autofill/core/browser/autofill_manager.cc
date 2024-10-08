// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
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
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
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

  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    if (!cached_form.field(i)->SameFieldAs(live_form.fields()[i])) {
      return true;
    }
  }

  return false;
}

}  // namespace

// static
void AutofillManager::LogTypePredictionsAvailable(
    LogManager* log_manager,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  LogBuffer buffer(IsLoggingActive(log_manager));
  for (FormStructure* form : forms)
    LOG_AF(buffer) << *form;

  LOG_AF(log_manager) << LoggingScope::kParsing << LogMessage::kParsedForms
                      << std::move(buffer);
}

AutofillManager::AutofillManager(AutofillDriver* driver)
    : driver_(CHECK_DEREF(driver)),
      log_manager_(client().GetLogManager()),
      form_interactions_ukm_logger_(CreateFormInteractionsUkmLogger()) {
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
  form_interactions_ukm_logger_ = CreateFormInteractionsUkmLogger();
}

void AutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (!base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection) ||
      !base::FeatureList::IsEnabled(features::kAutofillFixValueSemantics)) {
    return;
  }
  if (details.adopted_language == translate::kUnknownLanguageCode ||
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
  NotifyObservers(&Observer::OnFormSubmitted, form);
  OnFormSubmittedImpl(form, known_success, source);
}

void AutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  auto erase_removed_forms = [&]() {
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

  NotifyObservers(&Observer::OnBeforeFormsSeen, GetFormGlobalIds(updated_forms),
                  removed_forms);
  erase_removed_forms();

  auto ProcessParsedForms = [](std::vector<FormGlobalId> removed_forms,
                               AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty())
      self.OnFormsParsed(parsed_forms);
    self.NotifyObservers(&Observer::OnAfterFormsSeen,
                         GetFormGlobalIds(parsed_forms), removed_forms);
  };
  ParseFormsAsync(updated_forms,
                  base::BindOnce(ProcessParsedForms, std::move(removed_forms)));
}

void AutofillManager::OnFormsParsed(const std::vector<FormData>& forms) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  std::vector<raw_ptr<FormStructure, VectorExperimental>> non_queryable_forms;
  std::vector<raw_ptr<FormStructure, VectorExperimental>> queryable_forms;
  DenseSet<FormType> form_types;
  for (const FormData& form : forms) {
    FormStructure* form_structure = FindCachedFormById(form.global_id());
    if (!form_structure) {
      NOTREACHED_IN_MIGRATION();
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

  // Send the current type predictions to the renderer. For non-queryable forms
  // this is all the information about them that will ever be available. The
  // queryable forms will be updated once the field type query is complete.
  driver().SendTypePredictionsToRenderer(non_queryable_forms);
  driver().SendTypePredictionsToRenderer(queryable_forms);
  LogTypePredictionsAvailable(log_manager_, non_queryable_forms);
  LogTypePredictionsAvailable(log_manager_, queryable_forms);

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty() && client().GetCrowdsourcingManager()) {
    NotifyObservers(&Observer::OnBeforeLoadedServerPredictions);
    // If language detection is currently reparsing the form, wait until the
    // server response is processed, to ensure server predictions are not lost.
    client().GetCrowdsourcingManager()->StartQueryRequest(
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

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FieldGlobalId& field_id,
                                           const base::TimeTicks timestamp) {
  if (!IsValidFormData(form)) {
    return;
  }
  const FormFieldData& field = CHECK_DEREF(form.FindFieldByGlobalId(field_id));
  NotifyObservers(&Observer::OnBeforeTextFieldDidChange, form.global_id(),
                  field_id);
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::OnTextFieldDidChangeImpl,
                                 field_id, timestamp)
                     .Then(NotifyObserversCallback(
                         &Observer::OnAfterTextFieldDidChange, form.global_id(),
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

void AutofillManager::OnSelectControlDidChange(const FormData& form,
                                               const FieldGlobalId& field_id) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeSelectControlDidChange, form.global_id(),
                  field_id);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnSelectControlDidChangeImpl, field_id)
          .Then(
              NotifyObserversCallback(&Observer::OnAfterSelectControlDidChange,
                                      form.global_id(), field_id)));
}

void AutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  if (!IsValidFormData(form)) {
    return;
  }
  NotifyObservers(&Observer::OnBeforeAskForValuesToFill, form.global_id(),
                  field_id, form);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnAskForValuesToFillImpl, field_id,
                      caret_bounds, trigger_source)
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
  if (!IsValidFormData(form))
    return;
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnSelectFieldOptionsDidChangeImpl)
                .Then(NotifyNoObserversCallback()));
}

void AutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value,
    bool formatting_only) {
  if (!IsValidFormData(form))
    return;
  NotifyObservers(&Observer::OnBeforeJavaScriptChangedAutofilledValue,
                  form.global_id(), field_id);
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnJavaScriptChangedAutofilledValueImpl,
                      field_id, old_value, formatting_only)
          .Then(NotifyObserversCallback(
              &Observer::OnAfterJavaScriptChangedAutofilledValue,
              form.global_id(), field_id)));
}

bool AutofillManager::GetCachedFormAndField(
    const FormData& form,
    const FormFieldData& field,
    FormStructure** form_structure,
    AutofillField** autofill_field) const {
  FormStructure* cached_form = FindCachedFormById(form.global_id());
  // TODO: crbug.com/40232021 - Look into removing the `autofill_count() == 0`
  // disjunct. Because it is inconvenient that some code needs to tolerate null
  // FormStructures and/or AutofillFields because for Autocomplete still needs
  // to work if `autofill_count() == 0`. See
  // BrowserAutofillManager::AskForValuesToFillImpl() and
  // BrowserAutofillManager::FillOrPreviewField().
  if (!cached_form ||
      (cached_form->autofill_count() == 0 &&
       !base::FeatureList::IsEnabled(
           features::kAutofillDecoupleAutofillCountFromCache))) {
    return false;
  }
  *form_structure = cached_form;
  auto field_it = base::ranges::find(*cached_form, field.global_id(),
                                     &AutofillField::global_id);
  *autofill_field = field_it == cached_form->end() ? nullptr : field_it->get();
  return *autofill_field != nullptr;
}

std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
AutofillManager::CreateFormInteractionsUkmLogger() {
  return std::make_unique<AutofillMetrics::FormInteractionsUkmLogger>(
      &client(), client().GetUkmRecorder());
}

size_t AutofillManager::FindCachedFormsBySignature(
    FormSignature form_signature,
    std::vector<raw_ptr<FormStructure, VectorExperimental>>* form_structures)
    const {
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
  // TODO(crbug.com/40219607): We can't pass a UKM logger because it's a member
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
      if (base::FeatureList::IsEnabled(features::kAutofillFixValueSemantics)) {
        // Update the cache to the latest data from the renderer in the form
        // cache (in particular, the current field values) while preserving all
        // other information (in particular, the field types).
        form_structure->RetrieveFromCache(
            *cached_form_structure, FormStructure::RetrieveFromCacheReason::
                                        kFormCacheUpdateWithoutParsing);
        form_structures_[form_data.global_id()] = std::move(form_structure);
      }
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
  // TODO(crbug.com/40219607): We can't pass a UKM logger because it's a member
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
  // TODO(crbug.com/40232021): Make FormStructure's and FormData's fields
  // correspond, migrate all event handlers in BrowserAutofillManager take a
  // FormStructure, and drop the FormData from UpdateCache().
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
      response->queried_form_signatures, form_interactions_ukm_logger(),
      log_manager_);

  // Will log quality metrics for each FormStructure based on the presence of
  // autocomplete attributes, if available.
  if (auto* logger = form_interactions_ukm_logger()) {
    for (FormStructure* cur_form : queried_forms) {
      autofill_metrics::LogQualityMetricsBasedOnAutocomplete(*cur_form, logger);
    }
  }

  // Send field type predictions to the renderer so that it can possibly
  // annotate forms with the predicted types or add console warnings.
  driver().SendTypePredictionsToRenderer(queried_forms);
  LogTypePredictionsAvailable(log_manager_, queried_forms);

  for (const FormStructure* form : queried_forms) {
    NotifyObservers(&Observer::OnFieldTypesDetermined, form->global_id(),
                    Observer::FieldTypeSource::kAutofillServer);
  }
}

}  // namespace autofill
