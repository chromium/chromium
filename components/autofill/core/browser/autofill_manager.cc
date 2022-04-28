// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/feature_list.h"
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
#include "google_apis/google_api_keys.h"
#include "ui/gfx/geometry/rect_f.h"

namespace autofill {

namespace {

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

using base::TimeTicks;

// static
void AutofillManager::LogAutofillTypePredictionsAvailable(
    LogManager* log_manager,
    const std::vector<FormStructure*>& forms) {
  if (VLOG_IS_ON(1)) {
    VLOG(1) << "Parsed forms:";
    for (FormStructure* form : forms)
      VLOG(1) << *form;
  }

  if (!log_manager || !log_manager->IsLoggingActive())
    return;

  LogBuffer buffer;
  for (FormStructure* form : forms)
    buffer << *form;

  log_manager->Log() << LoggingScope::kParsing << LogMessage::kParsedForms
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
  translate_observation_.Reset();
}

void AutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  if (!base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)) {
    return;
  }
  for (auto& [form_id, form_structure] : form_structures_) {
    form_structure->set_current_page_language(
        LanguageCode(details.adopted_language));
    form_structure->DetermineHeuristicTypes(form_interactions_ukm_logger(),
                                            log_manager_);
  }
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

void AutofillManager::OnFormSubmitted(const FormData& form,
                                      bool known_success,
                                      mojom::SubmissionSource source) {
  if (IsValidFormData(form))
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

  if (!IsValidFormDataVector(updated_forms) || !driver_->RendererIsAvailable())
    return;

  // This should be called even forms is empty, AutofillProviderAndroid uses
  // this event to detect form submission.
  if (!ShouldParseForms(updated_forms))
    return;

  std::vector<const FormData*> new_forms;
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

    new_forms.push_back(&form);
    AutofillMetrics::LogParseFormTiming(AutofillTickClock::NowTicks() -
                                        parse_form_start_time);
  }

  if (new_forms.empty())
    return;
  OnFormsParsed(new_forms);
}

void AutofillManager::OnFormsParsed(const std::vector<const FormData*>& forms) {
  DCHECK(!forms.empty());
  OnBeforeProcessParsedForms();

  driver()->HandleParsedForms(forms);

  std::vector<FormStructure*> non_queryable_forms;
  std::vector<FormStructure*> queryable_forms;
  DenseSet<FormType> form_types;
  for (const FormData* form : forms) {
    FormStructure* form_structure =
        FindCachedFormByRendererId(form->global_id());
    if (!form_structure) {
      NOTREACHED();
      continue;
    }

    form_types.insert_all(form_structure->GetFormTypes());

    // Configure the query encoding for this form and add it to the appropriate
    // collection of forms: queryable vs non-queryable.
    if (form_structure->ShouldBeQueried())
      queryable_forms.push_back(form_structure);
    else
      non_queryable_forms.push_back(form_structure);

    OnFormProcessed(*form, *form_structure);
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
                                           const TimeTicks timestamp) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  OnTextFieldDidChangeImpl(form, field, bounding_box, timestamp);
}

void AutofillManager::OnTextFieldDidScroll(const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  OnTextFieldDidScrollImpl(form, field, bounding_box);
}

void AutofillManager::OnSelectControlDidChange(const FormData& form,
                                               const FormFieldData& field,
                                               const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  OnSelectControlDidChangeImpl(form, field, bounding_box);
}

void AutofillManager::OnAskForValuesToFill(int query_id,
                                           const FormData& form,
                                           const FormFieldData& field,
                                           const gfx::RectF& bounding_box,
                                           bool autoselect_first_suggestion) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  OnAskForValuesToFillImpl(query_id, form, field, bounding_box,
                           autoselect_first_suggestion);
}

void AutofillManager::OnFocusOnFormField(const FormData& form,
                                         const FormFieldData& field,
                                         const gfx::RectF& bounding_box) {
  if (!IsValidFormData(form) || !IsValidFormFieldData(field))
    return;

  OnFocusOnFormFieldImpl(form, field, bounding_box);
}

// Returns true if |live_form| does not match |cached_form|.
bool AutofillManager::GetCachedFormAndField(const FormData& form,
                                            const FormFieldData& field,
                                            FormStructure** form_structure,
                                            AutofillField** autofill_field) {
  // Maybe find an existing FormStructure that corresponds to |form|.
  FormStructure* cached_form = FindCachedFormByRendererId(form.global_id());
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

FormStructure* AutofillManager::ParseForm(const FormData& form,
                                          const FormStructure* cached_form) {
  if (form_structures_.size() >= kAutofillManagerMaxFormCacheSize) {
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
  driver()->PropagateAutofillPredictions(queried_forms);
}

void AutofillManager::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {}

}  // namespace autofill
