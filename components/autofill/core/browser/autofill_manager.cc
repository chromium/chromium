// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "google_apis/google_api_keys.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

// Creates a reply callback for ParseFormAsync().
//
// An event
//   AutofillManager::OnFoo(const FormData& form, args...)
// is handled by
// asynchronously parsing the form and then calling
//   AutofillManager::OnFooImpl(const FormData& form, args...)
// unless the AutofillManager has been destructed or reset in the meantime.
//
// ParsingCallback(&AutofillManager::OnFooImpl, args...) creates the
// corresponding callback to be passed to ParseFormAsync().
//
// The `after_event` is supposed to be &Observer::OnAfterFoo (or nullptr if no
// such event exists). The callback notifies the observers of `after_event`.
template <typename Functor, typename... Args>
base::OnceCallback<void(AutofillManager&, const FormData&)> ParsingCallback(
    Functor&& functor,
    void (AutofillManager::Observer::*after_event)(),
    Args&&... args) {
  return base::BindOnce(
      [](Functor&& functor, void (AutofillManager::Observer::*after_event)(),
         std::remove_reference_t<Args&&>... args, AutofillManager& self,
         const FormData& form) {
        base::invoke(std::forward<Functor>(functor), self, form,
                     std::forward<Args>(args)...);
        if (after_event)
          self.NotifyObservers(after_event);
      },
      std::forward<Functor>(functor), after_event, std::forward<Args>(args)...);
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

std::string GetAPIKeyForUrl(version_info::Channel channel) {
  // First look if we can get API key from command line flag.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kAutofillAPIKey))
    return command_line.GetSwitchValueASCII(switches::kAutofillAPIKey);

  // Get the API key from Chrome baked keys.
  if (channel == version_info::Channel::STABLE)
    return google_apis::GetAPIKey();
  return google_apis::GetNonStableAPIKey();
}

}  // namespace

// static
void AutofillManager::LogAutofillTypePredictionsAvailable(
    LogManager* log_manager,
    const std::vector<FormStructure*>& forms) {
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "Parsed forms:";
    for (FormStructure* form : forms)
      VLOG(1) << *form;
  }

  LogBuffer buffer(IsLoggingActive(log_manager));
  for (FormStructure* form : forms)
    LOG_AF(buffer) << *form;

  LOG_AF(log_manager) << LoggingScope::kParsing << LogMessage::kParsedForms
                      << std::move(buffer);
}

// static
bool AutofillManager::IsRawMetadataUploadingEnabled(
    version_info::Channel channel) {
  return channel == version_info::Channel::CANARY ||
         channel == version_info::Channel::DEV;
}

AutofillManager::AutofillManager(AutofillDriver* driver,
                                 AutofillClient* client,
                                 version_info::Channel channel,
                                 EnableDownloadManager enable_download_manager)
    : driver_(driver),
      client_(client),
      log_manager_(client ? client->GetLogManager() : nullptr),
      form_interactions_ukm_logger_(CreateFormInteractionsUkmLogger()) {
  if (enable_download_manager) {
    download_manager_ = std::make_unique<AutofillDownloadManager>(
        driver, this, GetAPIKeyForUrl(channel),
        AutofillDownloadManager::IsRawMetadataUploadingEnabled(
            IsRawMetadataUploadingEnabled(channel)),
        log_manager_);
  }
  if (client) {
    translate::TranslateDriver* translate_driver = client->GetTranslateDriver();
    if (translate_driver) {
      translate_observation_.Observe(translate_driver);
    }
  }
}

AutofillManager::~AutofillManager() {
  NotifyObservers(&Observer::OnAutofillManagerDestroyed);
  translate_observation_.Reset();
}

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
      form_structure->DetermineHeuristicTypes(form_interactions_ukm_logger(),
                                              log_manager_);
    }
    NotifyObservers(&Observer::OnAfterLanguageDetermined);
    return;
  }

  struct AsyncContext {
    AsyncContext(
        std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures,
        LogManager* log_manager)
        : form_structures(std::move(form_structures)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::map<FormGlobalId, std::unique_ptr<FormStructure>> form_structures;
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
          /*form_interactions_ukm_logger=*/nullptr, context.log_manager.get());
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto UpdateCache = [](base::WeakPtr<AutofillManager> self,
                        AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Autofill.Timing.OnLanguageDetermined.UpdateCache");
    if (!self)
      return;
    for (auto& [id, form_structure] : context.form_structures)
      self->form_structures_[id] = std::move(form_structure);
    if (context.log_manager && self->log_manager_)
      context.log_manager->Flush(*self->log_manager_);
    self->NotifyObservers(&Observer::OnAfterLanguageDetermined);
  };

  // Transfers ownership of the cached `form_structures_` to the worker task,
  // which will eventually move them back into `form_structures_`. This means
  // `AutofillManager::form_structures_` is empty for a brief period of time.
  auto form_structures = std::exchange(form_structures_, {});
  parsing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(RunHeuristics,
                     AsyncContext(std::move(form_structures), log_manager_)),
      base::BindOnce(UpdateCache, parsing_weak_ptr_factory_.GetWeakPtr()));
}

void AutofillManager::OnTranslateDriverDestroyed(
    translate::TranslateDriver* translate_driver) {
  translate_observation_.Reset();
}

LanguageCode AutofillManager::GetCurrentPageLanguage() {
  DCHECK(client());
  const translate::LanguageState* language_state = client()->GetLanguageState();
  if (!language_state)
    return LanguageCode();
  return LanguageCode(language_state->current_language());
}

void AutofillManager::FillCreditCardForm(const FormData& form,
                                         const FormFieldData& field,
                                         const CreditCard& credit_card,
                                         const std::u16string& cvc) {
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    FillCreditCardFormImpl(form, field, credit_card, cvc);
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::FillCreditCardFormImpl,
                            /*after_event=*/nullptr, field, credit_card, cvc));
}

void AutofillManager::FillProfileForm(const AutofillProfile& profile,
                                      const FormData& form,
                                      const FormFieldData& field) {
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    FillProfileFormImpl(form, field, profile);
    return;
  }
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::FillProfileFormImpl,
                                 /*after_event=*/nullptr, field, profile));
}

void AutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  if (!IsValidFormData(form))
    return;

  NotifyObservers(&Observer::OnBeforeDidFillAutofillFormData);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnDidFillAutofillFormDataImpl(form, timestamp);
    NotifyObservers(&Observer::OnAfterDidFillAutofillFormData);
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnDidFillAutofillFormDataImpl,
                      &Observer::OnAfterDidFillAutofillFormData, timestamp));
}

void AutofillManager::OnFormSubmitted(const FormData& form,
                                      const bool known_success,
                                      const mojom::SubmissionSource source) {
  if (!IsValidFormData(form))
    return;

  NotifyObservers(&Observer::OnBeforeFormSubmitted);
  NotifyObservers(&Observer::OnFormSubmitted);
  OnFormSubmittedImpl(form, known_success, source);
  NotifyObservers(&Observer::OnAfterFormSubmitted);
}

void AutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  // Erase forms that have been removed from the DOM. This prevents
  // |form_structures_| from growing up its upper bound
  // kAutofillManagerMaxFormCacheSize.
  for (FormGlobalId removed_form : removed_forms)
    form_structures_.erase(removed_form);

  if (!IsValidFormDataVector(updated_forms) || !driver_->RendererIsAvailable())
    return;

  // This should be called even forms is empty, AutofillProviderAndroid uses
  // this event to detect form submission.
  if (!ShouldParseForms(updated_forms))
    return;

  NotifyObservers(&Observer::OnBeforeFormsSeen);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    std::vector<FormData> parsed_forms;
    for (const FormData& form : updated_forms) {
      const auto parse_form_start_time = AutofillTickClock::NowTicks();
      FormStructure* cached_form_structure =
          FindCachedFormByRendererId(form.global_id());

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

      if (update_form_signature)
        form_structure->set_form_signature(CalculateFormSignature(form));

      parsed_forms.push_back(form);
      AutofillMetrics::LogParseFormTiming(AutofillTickClock::NowTicks() -
                                          parse_form_start_time);
    }
    if (!parsed_forms.empty())
      OnFormsParsed(parsed_forms);
    NotifyObservers(&Observer::OnAfterFormsSeen);
    return;
  }
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillParseAsync));
  auto ProcessParsedForms = [](AutofillManager& self,
                               const std::vector<FormData>& parsed_forms) {
    if (!parsed_forms.empty())
      self.OnFormsParsed(parsed_forms);
    self.NotifyObservers(&Observer::OnAfterFormsSeen);
  };
  ParseFormsAsync(updated_forms, base::BindOnce(ProcessParsedForms));
}

void AutofillManager::OnFormsParsed(const std::vector<FormData>& forms) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  driver()->HandleParsedForms(forms);

  std::vector<FormStructure*> non_queryable_forms;
  std::vector<FormStructure*> queryable_forms;
  DenseSet<FormType> form_types;
  for (const FormData& form : forms) {
    FormStructure* form_structure =
        FindCachedFormByRendererId(form.global_id());
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
  driver()->SendAutofillTypePredictionsToRenderer(non_queryable_forms);
  driver()->SendAutofillTypePredictionsToRenderer(queryable_forms);
  // Send the fields that are eligible for manual filling to the renderer. If
  // server predictions are not yet available for these forms, the eligible
  // fields would be updated again once they are available.
  driver()->SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(non_queryable_forms));
  driver()->SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(queryable_forms));
  LogAutofillTypePredictionsAvailable(log_manager_, non_queryable_forms);
  LogAutofillTypePredictionsAvailable(log_manager_, queryable_forms);

  // Query the server if at least one of the forms was parsed.
  if (!queryable_forms.empty() && download_manager()) {
    download_manager()->StartQueryRequest(queryable_forms);
  }
}

void AutofillManager::OnTextFieldDidChange(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           const base::TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeTextFieldDidChange);
  NotifyObservers(&Observer::OnTextFieldDidChange);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnTextFieldDidChangeImpl(form, field, bounding_box, timestamp);
    NotifyObservers(&Observer::OnAfterTextFieldDidChange);
    return;
  }
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::OnTextFieldDidChangeImpl,
                                 &Observer::OnAfterTextFieldDidChange, field,
                                 bounding_box, timestamp));
}

void AutofillManager::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnTextFieldDidScroll);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnTextFieldDidScrollImpl(form, field, bounding_box);
    return;
  }
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::OnTextFieldDidScrollImpl,
                                 /*after_event=*/nullptr, field, bounding_box));
}

void AutofillManager::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnSelectControlDidChange);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnSelectControlDidChangeImpl(form, field, bounding_box);
    return;
  }
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::OnSelectControlDidChangeImpl,
                                 /*after_event=*/nullptr, field, bounding_box));
}

void AutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    FormElementWasClicked form_element_was_clicked) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  NotifyObservers(&Observer::OnBeforeAskForValuesToFill);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)
#if BUILDFLAG(IS_ANDROID)
      // TODO(crbug.com/1379149) Asynchronous parsing breaks Touch To Fill's the
      // keyboard suppression mechanism. Fast Checkout uses the same mechanism.
      // Also see crbug.com/1375966.
      || client_->IsTouchToFillCreditCardSupported() ||
      client_->IsFastCheckoutSupported()
#endif
  ) {
    OnAskForValuesToFillImpl(form, field, bounding_box,
                             autoselect_first_suggestion,
                             form_element_was_clicked);
    NotifyObservers(&Observer::OnAfterAskForValuesToFill);
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnAskForValuesToFillImpl,
                      &Observer::OnAfterAskForValuesToFill, field, bounding_box,
                      autoselect_first_suggestion, form_element_was_clicked));
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
  ParseFormAsync(form,
                 ParsingCallback(&AutofillManager::OnFocusOnFormFieldImpl,
                                 /*after_event=*/nullptr, field, bounding_box));
}

void AutofillManager::OnFocusNoLongerOnForm(bool had_interacted_form) {
  OnFocusNoLongerOnFormImpl(had_interacted_form);
}

void AutofillManager::OnDidPreviewAutofillFormData() {
  OnDidPreviewAutofillFormDataImpl();
}

void AutofillManager::OnDidEndTextFieldEditing() {
  OnDidEndTextFieldEditingImpl();
}

void AutofillManager::OnHidePopup() {
  OnHidePopupImpl();
}

void AutofillManager::OnSelectFieldOptionsDidChange(const FormData& form) {
  if (!IsValidFormData(form))
    return;

  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnSelectFieldOptionsDidChangeImpl(form);
    return;
  }
  ParseFormAsync(
      form, ParsingCallback(&AutofillManager::OnSelectFieldOptionsDidChangeImpl,
                            /*after_event=*/nullptr));
}

void AutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FormFieldData& field,
    const std::u16string& old_value) {
  if (!IsValidFormData(form))
    return;

  NotifyObservers(&Observer::OnBeforeAskForValuesToFill);
  if (!base::FeatureList::IsEnabled(features::kAutofillParseAsync)) {
    OnJavaScriptChangedAutofilledValueImpl(form, field, old_value);
    NotifyObservers(&Observer::OnAfterAskForValuesToFill);
    return;
  }
  ParseFormAsync(
      form,
      ParsingCallback(&AutofillManager::OnJavaScriptChangedAutofilledValueImpl,
                      &Observer::OnAfterAskForValuesToFill, field, old_value));
}

// Returns true if |live_form| does not match |cached_form|.
bool AutofillManager::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Maybe find an existing FormStructure that corresponds to |form|.
  FormStructure* cached_form = FindCachedFormByRendererId(form.global_id());
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
  driver()->SendAutofillTypePredictionsToRenderer({*form_structure});
  // Update the renderer with the latest set of fields eligible for manual
  // filling.
  driver()->SendFieldsEligibleForManualFillingToRenderer(
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
  if (!unsafe_client())
    return nullptr;

  return std::make_unique<AutofillMetrics::FormInteractionsUkmLogger>(
      unsafe_client()->GetUkmRecorder(), unsafe_client()->GetUkmSourceId());
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

FormStructure* AutofillManager::FindCachedFormByRendererId(
    FormGlobalId form_id) const {
  auto it = form_structures_.find(form_id);
  return it != form_structures_.end() ? it->second.get() : nullptr;
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
            FindCachedFormByRendererId(form_data.global_id())) {
      // We need to keep the server data if available. We need to use them while
      // determining the heuristics.
      form_structure->RetrieveFromCache(
          *cached_form_structure,
          FormStructure::RetrieveFromCacheReason::kFormParsing);
      if (form_structure->value_from_dynamic_change_form())
        value_from_dynamic_change_form_ = true;

      // Not updating signatures of credit card forms is legacy behaviour. We
      // believe that the signatures are kept stable for voting purposes.
      DenseSet<FormType> form_types = cached_form_structure->GetFormTypes();
      if (form_types.size() > form_types.count(FormType::kCreditCardForm))
        form_structure->set_form_signature(CalculateFormSignature(form_data));
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
                 LogManager* log_manager)
        : form_structures(std::move(form_structures)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::vector<std::unique_ptr<FormStructure>> form_structures;
    std::unique_ptr<BufferingLogManager> log_manager;
  };

  // To be run on a different task (must not access global or member
  // variables).
  // TODO(crbug.com/1309848): We can't pass a UKM logger because it's a member
  // variable. To be fixed.
  auto RunHeuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormsAsync.RunHeuristics");
    for (auto& form_structure : context.form_structures) {
      form_structure->DetermineHeuristicTypes(
          /*form_interactions_ukm_logger=*/nullptr, context.log_manager.get());
    }
    return context;
  };

  // To be run on the main thread (accesses member variables).
  auto UpdateCache =
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(AutofillManager&,
                                 const std::vector<FormData>&)> callback,
         const std::vector<FormData>& parsed_forms, AsyncContext context) {
        SCOPED_UMA_HISTOGRAM_TIMER(
            "Autofill.Timing.ParseFormsAsync.UpdateCache");
        if (!self)
          return;
        for (auto& form_structure : context.form_structures) {
          FormGlobalId id = form_structure->global_id();
          self->form_structures_[id] = std::move(form_structure);
        }
        if (context.log_manager && self->log_manager_)
          context.log_manager->Flush(*self->log_manager_);
        self->NotifyObservers(&Observer::OnFormParsed);
        std::move(callback).Run(*self, parsed_forms);
      };

  parsing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(RunHeuristics,
                     AsyncContext(std::move(form_structures), log_manager_)),
      base::BindOnce(UpdateCache, parsing_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), parsed_forms));
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
          FindCachedFormByRendererId(form_data.global_id())) {
    if (!CachedFormNeedsUpdate(form_data, *cached_form_structure)) {
      std::move(callback).Run(*this, form_data);
      return;
    }

    // We need to keep the server data if available. We need to use them while
    // determining the heuristics.
    form_structure->RetrieveFromCache(
        *cached_form_structure,
        FormStructure::RetrieveFromCacheReason::kFormParsing);
    if (form_structure->value_from_dynamic_change_form())
      value_from_dynamic_change_form_ = true;
  }
  form_structure->set_current_page_language(GetCurrentPageLanguage());

  struct AsyncContext {
    AsyncContext(std::unique_ptr<FormStructure> form_structure,
                 LogManager* log_manager)
        : form_structure(std::move(form_structure)),
          log_manager(IsLoggingActive(log_manager)
                          ? LogManager::CreateBuffering()
                          : nullptr) {}
    std::unique_ptr<FormStructure> form_structure;
    std::unique_ptr<BufferingLogManager> log_manager;
  };

  // To be run on a different task (must not access global or member
  // variables).
  // TODO(crbug.com/1309848): We can't pass a UKM logger because it's a member
  // variable. To be fixed.
  auto RunHeuristics = [](AsyncContext context) {
    SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.ParseFormAsync.RunHeuristics");
    context.form_structure->DetermineHeuristicTypes(
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
  auto UpdateCache =
      [](base::WeakPtr<AutofillManager> self,
         base::OnceCallback<void(AutofillManager&, const FormData&)> callback,
         const FormData& form_data, AsyncContext context) {
        SCOPED_UMA_HISTOGRAM_TIMER(
            "Autofill.Timing.ParseFormAsync.UpdateCache");
        if (!self)
          return;
        FormGlobalId id = context.form_structure->global_id();
        self->form_structures_[id] = std::move(context.form_structure);
        if (context.log_manager && self->log_manager_)
          context.log_manager->Flush(*self->log_manager_);
        self->NotifyObservers(&Observer::OnFormParsed);
        std::move(callback).Run(*self, form_data);
      };

  parsing_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(RunHeuristics,
                     AsyncContext(std::move(form_structure), log_manager_)),
      base::BindOnce(UpdateCache, parsing_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), form_data));
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

    NotifyObservers(&Observer::OnFormParsed);
    if (form_structure.get()->value_from_dynamic_change_form())
      value_from_dynamic_change_form_ = true;
  }

  form_structure->set_current_page_language(GetCurrentPageLanguage());

  form_structure->DetermineHeuristicTypes(form_interactions_ukm_logger(),
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
  if (queried_forms.empty())
    return;

  // Parse and store the server predictions.
  FormStructure::ParseApiQueryResponse(
      std::move(response), queried_forms, queried_form_signatures,
      form_interactions_ukm_logger(), log_manager_);

  // Will log quality metrics for each FormStructure based on the presence of
  // autocomplete attributes, if available.
  if (auto* logger = form_interactions_ukm_logger()) {
    for (FormStructure* cur_form : queried_forms) {
      cur_form->LogQualityMetricsBasedOnAutocomplete(logger);
    }
  }

  // Send field type predictions to the renderer so that it can possibly
  // annotate forms with the predicted types or add console warnings.
  driver()->SendAutofillTypePredictionsToRenderer(queried_forms);

  driver()->SendFieldsEligibleForManualFillingToRenderer(
      FormStructure::FindFieldsEligibleForManualFilling(queried_forms));

  LogAutofillTypePredictionsAvailable(log_manager_, queried_forms);

  // Forward form structures to the password generation manager to detect
  // account creation forms.
  PropagateAutofillPredictions(queried_forms);
}

void AutofillManager::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {}

}  // namespace autofill
