// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_processing/label_processing_util.h"
#include "components/autofill/core/browser/form_processing/name_processing_util.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/server_prediction_overrides.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/version_info.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

// Returns true if the scheme given by |url| is one for which autofill is
// allowed to activate. By default this only returns true for HTTP and HTTPS.
bool HasAllowedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS();
}

std::string ServerTypesToString(const AutofillField* field) {
  const std::vector<
      AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction>&
      server_types = field->server_predictions();

  if (server_types.empty()) {
    return "pending";
  }

  std::ostringstream buffer;
  for (const auto& field_prediction : server_types) {
    if (buffer.tellp() > 0) {  // Add comma if buffer is not empty.
      buffer << ", ";
    }
    FieldType server_type =
        ToSafeFieldType(field_prediction.type(), NO_SERVER_DATA);
    buffer << FieldTypeToStringView(server_type);
  }
  return "[" + buffer.str() + "]";
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : id_attribute_(form.id_attribute()),
      name_attribute_(form.name_attribute()),
      form_name_(form.name()),
      button_titles_(form.button_titles()),
      source_url_(form.url()),
      full_source_url_(form.full_url()),
      target_url_(form.action()),
      main_frame_origin_(form.main_frame_origin()),
      all_fields_are_passwords_(!form.fields().empty()),
      form_parsed_timestamp_(base::TimeTicks::Now()),
      host_frame_(form.host_frame()),
      version_(form.version()),
      renderer_id_(form.renderer_id()),
      child_frames_(form.child_frames()) {
  // Copy the form fields.
  for (const FormFieldData& field : form.fields()) {
    if (!IsCheckable(field.check_status())) {
      ++active_field_count_;
    }

    if (field.form_control_type() == FormControlType::kInputPassword) {
      has_password_field_ = true;
    } else {
      all_fields_are_passwords_ = false;
    }

    fields_.push_back(std::make_unique<AutofillField>(field));
  }

  form_signature_ = CalculateFormSignature(form);
  alternative_form_signature_ = CalculateAlternativeFormSignature(form);
  // Do further processing on the fields, as needed.
  ProcessExtractedFields();
  SetFieldTypesFromAutocompleteAttribute();
  DetermineFieldRanks();
}

FormStructure::FormStructure(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures)
    : form_signature_(form_signature) {
  for (const auto& signature : field_signatures)
    fields_.push_back(AutofillField::CreateForPasswordManagerUpload(signature));
  DetermineFieldRanks();
}

FormStructure::~FormStructure() = default;

void FormStructure::DetermineFieldRanks() {
  size_t rank = 0;
  std::map<FormGlobalId, size_t> rank_in_host_form;
  std::map<FieldSignature, size_t> rank_in_signature_group;
  std::map<std::pair<FormGlobalId, FieldSignature>, size_t>
      rank_in_host_form_signature_group;

  for (auto& field : fields_) {
    field->set_rank(rank++);
    field->set_rank_in_host_form(
        rank_in_host_form[field->renderer_form_id()]++);
    field->set_rank_in_signature_group(
        rank_in_signature_group[field->GetFieldSignature()]++);
    field->set_rank_in_host_form_signature_group(
        rank_in_host_form_signature_group[std::make_pair(
            field->renderer_form_id(), field->GetFieldSignature())]++);
  }
}

void FormStructure::DetermineHeuristicTypes(
    const GeoIpCountryCode& client_country,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DetermineHeuristicTypes");

  client_country_ = client_country;

  const LanguageCode& page_language =
      base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)
          ? current_page_language_
          : LanguageCode();
  // The `PatternFile` parameter is overwritten again immediately before
  // parsing and doesn't matter here.
  ParsingContext context(client_country_, page_language,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                         PatternFile::kDefault,
#else
                         PatternFile::kLegacy,
#endif
                         GetActiveRegexFeatures(), log_manager);

  // The active heuristic source might not be a pattern source.
  std::optional<FieldCandidatesMap> active_predictions;
  HeuristicSource active_heuristic_source = GetActiveHeuristicSource();
  if (std::optional<PatternFile> pattern_file =
          HeuristicSourceToPatternFile(active_heuristic_source)) {
    context.pattern_file = *pattern_file;
    active_predictions = ParseFieldTypesWithPatterns(context);
    AssignBestFieldTypes(*active_predictions, active_heuristic_source);
  }
  DetermineNonActiveHeuristicTypes(std::move(active_predictions), context);

  UpdateAutofillCount();
  AssignSections(fields_);

  FormStructureRationalizer rationalizer(&fields_);
  rationalizer.RationalizeContentEditables(log_manager);
  rationalizer.RationalizeAutocompleteAttributes(log_manager);
  if (base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)) {
    rationalizer.RationalizeRepeatedFields(
        form_signature_, form_interactions_ukm_logger, log_manager);
  }
  rationalizer.RationalizeFieldTypePredictions(
      main_frame_origin_, client_country_, current_page_language_, log_manager);

  // Log the field type predicted by rationalization.
  // The sections are mapped to consecutive natural numbers starting at 1.
  std::map<Section, size_t> section_id_map;
  for (const auto& field : fields_) {
    if (!base::Contains(section_id_map, field->section())) {
      size_t next_section_id = section_id_map.size() + 1;
      section_id_map[field->section()] = next_section_id;
    }
    field->AppendLogEventIfNotRepeated(RationalizationFieldLogEvent{
        .field_type = field->Type().GetStorableType(),
        .section_id = section_id_map[field->section()],
        .type_changed = field->Type().GetStorableType() !=
                        field->ComputedType().GetStorableType(),
    });
  }

  LogDetermineHeuristicTypesMetrics();
}

void FormStructure::DetermineNonActiveHeuristicTypes(
    std::optional<FieldCandidatesMap> active_predictions,
    ParsingContext& context) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableImprovedPredictionParser)) {
    // Run the parser for the prediction improvements.
    context.pattern_file = PatternFile::kPredictionImprovements;
    AssignBestFieldTypes(ParseFieldTypesWithPatterns(context),
                         HeuristicSource::kPredictionImprovementRegexes);
  }

  if (GetActiveHeuristicSource() == HeuristicSource::kDefaultRegexes) {
    return;
  }
  // When a non-default source is active, shadow predictions between this
  // non-default source and default are emitted. Compute default predictions.
  context.pattern_file = PatternFile::kDefault;
  context.active_features.clear();
  AssignBestFieldTypes(ParseFieldTypesWithPatterns(context),
                       HeuristicSource::kDefaultRegexes);
#endif
}

// static
std::vector<FormDataPredictions> FormStructure::GetFieldTypePredictions(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>&
        form_structures) {
  std::vector<FormDataPredictions> forms;
  forms.reserve(form_structures.size());
  for (const FormStructure* form_structure : form_structures) {
    FormDataPredictions form;
    form.data = form_structure->ToFormData();
    form.signature = form_structure->FormSignatureAsStr();
    form.alternative_signature = base::NumberToString(
        form_structure->alternative_form_signature().value());

    for (const auto& field : form_structure->fields_) {
      FormFieldDataPredictions annotated_field;
      annotated_field.host_form_signature =
          base::NumberToString(field->host_form_signature().value());
      annotated_field.signature = field->FieldSignatureAsStr();
      annotated_field.heuristic_type =
          FieldTypeToStringView(field->heuristic_type());
      if (!field->server_predictions().empty()) {
        annotated_field.server_type =
            FieldTypeToStringView(field->server_type());
      }
      annotated_field.html_type = FieldTypeToStringView(field->html_type());
      annotated_field.overall_type = std::string(field->Type().ToStringView());
      annotated_field.parseable_name =
          base::UTF16ToUTF8(field->parseable_name());
      annotated_field.section = field->section().ToString();
      annotated_field.rank = field->rank();
      annotated_field.rank_in_signature_group =
          field->rank_in_signature_group();
      annotated_field.rank_in_host_form = field->rank_in_host_form();
      annotated_field.rank_in_host_form_signature_group =
          field->rank_in_host_form_signature_group();
      form.fields.push_back(annotated_field);
    }

    forms.push_back(form);
  }
  return forms;
}

// static
std::vector<FieldGlobalId> FormStructure::FindFieldsEligibleForManualFilling(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
  std::vector<FieldGlobalId> fields_eligible_for_manual_filling;
  for (const autofill::FormStructure* form : forms) {
    for (const auto& field : form->fields_) {
      FieldTypeGroup field_type_group =
          GroupTypeOfFieldType(field->server_type());
      // In order to trigger the payments bottom sheet that assists users to
      // manually fill the form, credit card form fields are marked eligible for
      // manual filling. Also, if a field is not classified to a type, we can
      // assume that the prediction failed and thus mark it eligible for manual
      // filling. As more form types support manual filling on form interaction,
      // this list may expand in the future.
      if (field_type_group == FieldTypeGroup::kCreditCard ||
          field_type_group == FieldTypeGroup::kNoGroup) {
        fields_eligible_for_manual_filling.push_back(field->global_id());
      }
    }
  }
  return fields_eligible_for_manual_filling;
}

std::unique_ptr<FormStructure> FormStructure::CreateForPasswordManagerUpload(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures) {
  return base::WrapUnique(new FormStructure(form_signature, field_signatures));
}

std::string FormStructure::FormSignatureAsStr() const {
  return base::NumberToString(form_signature().value());
}

bool FormStructure::IsAutofillable() const {
  size_t min_required_fields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
  if (autofill_count() < min_required_fields)
    return false;

  return ShouldBeParsed();
}

bool FormStructure::IsCompleteCreditCardForm() const {
  bool found_cc_number = false;
  bool found_cc_expiration = false;
  for (const auto& field : fields_) {
    FieldType type = field->Type().GetStorableType();
    if (!found_cc_expiration && data_util::IsCreditCardExpirationType(type)) {
      found_cc_expiration = true;
    } else if (!found_cc_number && type == CREDIT_CARD_NUMBER) {
      found_cc_number = true;
    }
    if (found_cc_expiration && found_cc_number)
      return true;
  }
  return false;
}

void FormStructure::UpdateAutofillCount() {
  autofill_count_ = 0;
  for (const auto& field : *this) {
    if (field && field->IsFieldFillable())
      ++autofill_count_;
  }
}

bool FormStructure::ShouldBeParsed(ShouldBeParsedParams params,
                                   LogManager* log_manager) const {
  // Exclude URLs not on the web via HTTP(S).
  if (!HasAllowedScheme(source_url_)) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotAllowedScheme << *this;
    return false;
  }

  if (active_field_count() < params.min_required_fields &&
      (!all_fields_are_passwords() ||
       active_field_count() <
           params.required_fields_for_forms_with_only_password_fields) &&
      !has_author_specified_types_) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotEnoughFields
                        << active_field_count() << *this;
    return false;
  }

  // Rule out search forms.
  if (MatchesRegex<kUrlSearchActionRe>(
          base::UTF8ToUTF16(target_url_.path_piece()))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingUrlMatchesSearchRegex
                        << *this;
    return false;
  }

  bool has_text_field = std::ranges::any_of(
      *this, [](const auto& field) { return !field->IsSelectElement(); });
  if (!has_text_field) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingFormHasNoTextfield << *this;
  }
  return has_text_field;
}

bool FormStructure::ShouldRunHeuristics() const {
  return active_field_count() >= kMinRequiredFieldsForHeuristics &&
         HasAllowedScheme(source_url_);
}

bool FormStructure::ShouldRunHeuristicsForSingleFieldForms() const {
  return active_field_count() > 0 && HasAllowedScheme(source_url_);
}

bool FormStructure::ShouldBeQueried() const {
  return (has_password_field_ ||
          active_field_count() >= kMinRequiredFieldsForQuery) &&
         ShouldBeParsed();
}

bool FormStructure::ShouldBeUploaded() const {
  return active_field_count() >= kMinRequiredFieldsForUpload &&
         ShouldBeParsed();
}

void FormStructure::RetrieveFromCache(const FormStructure& cached_form,
                                      RetrieveFromCacheReason reason) {
  // Build a table to lookup AutofillFields by their FieldGlobalId.
  auto cached_fields_by_id =
      base::MakeFlatMap<FieldGlobalId, const AutofillField*>(
          cached_form.fields(), {},
          [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(), field.get());
          });

  // Lookup field by global_id in cached_fields_by_id.
  auto find_field_by_id = [&cached_fields_by_id](FieldGlobalId global_id) {
    const auto& it = cached_fields_by_id.find(global_id);
    return it != cached_fields_by_id.end() ? it->second : nullptr;
  };

  // Lookup field by field signature and return it in case only a single field
  // with the signature exists.
  auto find_field_with_unique_field_signature =
      [&cached_fields_by_id](
          FieldSignature field_signature) -> const AutofillField* {
    const AutofillField* match = nullptr;
    // Iterate over the fields to find the field with the same form signature.
    for (const auto& entry : cached_fields_by_id) {
      if (entry.second->GetFieldSignature() == field_signature) {
        // If there are multiple matches, do not retrieve the field and stop
        // the process.
        if (match)
          return nullptr;
        match = entry.second;
      }
    }
    return match;
  };

  for (auto& field : *this) {
    const AutofillField* cached_field = find_field_by_id(field->global_id());

    // If the unique renderer id (or the name) is not stable due to some Java
    // Script magic in the website, use the field signature as a fallback
    // solution to find the field in the cached form.
    if (!cached_field) {
      cached_field =
          find_field_with_unique_field_signature(field->GetFieldSignature());
    }

    // Skip fields that we could not find.
    if (!cached_field)
      continue;

    // TODO: crbug.com/40227496 - Simplify the `switch` statement once
    // kAutofillFixValueSemantics is launched.
    // TODO: crbug.com/40227496 - Remove the IsSelectElement()
    // checks once kAutofillFixValueSemantics is launched.
    // TODO: crbug.com/40227496 - Update the comments when the experiments are
    // launched.
    switch (reason) {
      case RetrieveFromCacheReason::kFormCacheUpdateAfterParsing:
      case RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing:
        // If kAutofillFixValueSemantics is disabled: During form parsing (as in
        // "assigning field types to fields") the `value` represents the initial
        // value found at page load and needs to be preserved.
        if (!field->IsSelectElement() ||
            base::FeatureList::IsEnabled(
                features::kAutofillFixInitialValueOfSelect)) {
          field->set_initial_value(
              cached_field->value(ValueSemantics::kInitial), /*pass_key=*/{});
        }
        break;
      case RetrieveFromCacheReason::kFormImport:
        // TODO: crbug.com/40227496 - Group IsSelectElement() checks.
        if ((!field->IsSelectElement() ||
             base::FeatureList::IsEnabled(
                 features::kAutofillFixInitialValueOfSelect)) &&
            base::FeatureList::IsEnabled(
                features::kAutofillFixValueSemantics)) {
          field->set_initial_value(
              cached_field->value(ValueSemantics::kInitial), /*pass_key=*/{});
        }
        // From the perspective of learning user data, text fields containing
        // default values are equivalent to empty fields. So if the value of
        // a submitted form corresponds to the initial value of the field, we
        // clear that value.
        // Since a website can prefill country and state values based on
        // GeoIP, we want to hold on to these values.
        const bool same_value_as_on_page_load =
            field->value(ValueSemantics::kCurrent) ==
            cached_field->value(ValueSemantics::kInitial);
        const bool had_type =
            cached_field->Type().GetStorableType() > FieldType::UNKNOWN_TYPE ||
            !cached_field->possible_types().empty();
        if (!cached_field->value(ValueSemantics::kInitial).empty() &&
            (!field->IsSelectElement() ||
             base::FeatureList::IsEnabled(
                 features::kAutofillFixInitialValueOfSelect)) &&
            had_type) {
          field->set_initial_value_changed(!same_value_as_on_page_load);
        }
        // TODO: crbug.com/40137859 - The server type hasn't been set yet (it is
        // set a few lines further down below), so this is trivially true. Once
        // kAutofillFixCurrentValueInImport is launched, consider adding this
        // to AutofillField::value_for_import() and running an experiment for
        // this.
        const bool field_is_neither_state_nor_country =
            field->server_type() != ADDRESS_HOME_COUNTRY &&
            field->server_type() != ADDRESS_HOME_STATE;
        if ((!field->IsSelectElement() &&
             !base::FeatureList::IsEnabled(
                 features::kAutofillFixCurrentValueInImport)) &&
            same_value_as_on_page_load && field_is_neither_state_nor_country) {
          field->set_value(std::u16string());
        }
        break;
    }

    field->set_server_predictions(cached_field->server_predictions());

    // Preserve state whether the field was autofilled before.
    if (reason == RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing ||
        reason == RetrieveFromCacheReason::kFormCacheUpdateAfterParsing) {
      field->set_is_autofilled(cached_field->is_autofilled());
    }
    field->set_autofill_source_profile_guid(
        cached_field->autofill_source_profile_guid());
    field->set_autofilled_type(cached_field->autofilled_type());
    field->set_filling_product(cached_field->filling_product());
    field->set_may_use_prefilled_placeholder(
        cached_field->may_use_prefilled_placeholder());
    field->set_previously_autofilled(cached_field->previously_autofilled());
    field->set_did_trigger_suggestions(cached_field->did_trigger_suggestions());
    field->set_was_focused(cached_field->was_focused());

    // During form parsing, we don't care for heuristic field classifications
    // and information derived from the autocomplete attribute as those are
    // either regenerated or copied from the form that the renderer sent.
    // During import, no parsing happens and we want to preserve the last field
    // classification. Similarly, if the renderer sends an update that does not
    // trigger parsing, we want to preserve the last field classification
    if (reason == RetrieveFromCacheReason::kFormCacheUpdateWithoutParsing ||
        reason == RetrieveFromCacheReason::kFormImport) {
      // Transfer attributes of the cached AutofillField to the newly created
      // AutofillField.
      for (int i = 0; i <= static_cast<int>(HeuristicSource::kMaxValue); ++i) {
        HeuristicSource s = static_cast<HeuristicSource>(i);
        field->set_heuristic_type(s, cached_field->heuristic_type(s));
      }
      field->SetHtmlType(cached_field->html_type(), cached_field->html_mode());
      field->set_section(cached_field->section());
      field->set_only_fill_when_focused(cached_field->only_fill_when_focused());

      // During import, the final field type is used to decide which
      // information to store in an address profile or credit card. As
      // rationalization is an important component of determining the final
      // field type, the output should be preserved.
      field->SetTypeTo(cached_field->Type());
    }
    field->set_field_log_events(cached_field->field_log_events());
  }

  UpdateAutofillCount();

  // Preserve timestamp from the cache as a new form from the renderer does not
  // know the parsing/filling history, as this information is computed in the
  // browser.
  form_parsed_timestamp_ =
      std::min(form_parsed_timestamp_, cached_form.form_parsed_timestamp_);
  last_filling_timestamp_ = cached_form.last_filling_timestamp_;

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  form_signature_ = cached_form.form_signature_;
}

void FormStructure::LogDetermineHeuristicTypesMetrics() {
  developer_engagement_metrics_ = 0;
  if (IsAutofillable()) {
    AutofillMetrics::DeveloperEngagementMetric metric =
        has_author_specified_types_
            ? AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS
            : AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS;
    developer_engagement_metrics_ |= 1 << metric;
    AutofillMetrics::LogDeveloperEngagementMetric(metric);
  }
}

void FormStructure::SetFieldTypesFromAutocompleteAttribute() {
  has_author_specified_types_ = false;
  std::map<FieldSignature, size_t> field_rank_map;
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    if (!field->parsed_autocomplete()) {
      continue;
    }

    // A parsable autocomplete value was specified. Even an invalid field_type
    // is considered a type hint. This allows a website's author to specify an
    // attribute like autocomplete="other" on a field to disable all Autofill
    // heuristics for the form.
    has_author_specified_types_ = true;
    if (field->parsed_autocomplete()->field_type ==
        HtmlFieldType::kUnspecified) {
      continue;
    }

    field->SetHtmlType(field->parsed_autocomplete()->field_type,
                       field->parsed_autocomplete()->mode);

    // Log the field type predicted from autocomplete attribute.
    ++field_rank_map[field->GetFieldSignature()];
    field->AppendLogEventIfNotRepeated(AutocompleteAttributeFieldLogEvent{
        .html_type = field->parsed_autocomplete()->field_type,
        .html_mode = field->parsed_autocomplete()->mode,
        .rank_in_field_signature_group =
            field_rank_map[field->GetFieldSignature()],
    });
  }
}

bool FormStructure::SetSectionsFromAutocompleteOrReset() {
  bool has_autocomplete = false;
  for (const auto& field : fields_) {
    if (!field->parsed_autocomplete()) {
      field->set_section(Section());
      continue;
    }

    field->set_section(Section::FromAutocomplete(
        {.section = field->parsed_autocomplete()->section,
         .mode = field->parsed_autocomplete()->mode}));
    if (field->section()) {
      has_autocomplete = true;
    }
  }
  return has_autocomplete;
}

FieldCandidatesMap FormStructure::ParseFieldTypesWithPatterns(
    ParsingContext& context) const {
  FieldCandidatesMap field_type_map;

  if (ShouldRunHeuristics()) {
    FormFieldParser::ParseFormFields(context, fields_, is_form_element(),
                                     field_type_map);
  } else if (ShouldRunHeuristicsForSingleFieldForms()) {
    FormFieldParser::ParseSingleFieldForms(context, fields_, field_type_map);
    FormFieldParser::ParseStandaloneCVCFields(context, fields_, field_type_map);

    // For standalone email fields, allow heuristics even when the minimum
    // number of fields is not met. See similar comments in
    // `FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields`.
    // Note that `kAutofillEnableEmailHeuristicOnlyAddressForms` only supports
    // email fields inside a form tag. Once
    // `kAutofillEnableEmailHeuristicOnlyAddressForms` launches, dropping this
    // requirement will be launched via
    // `kAutofillEnableEmailHeuristicOutsideForms`.
    const bool parse_standalone_email_fields =
        (is_form_element() ||
         base::FeatureList::IsEnabled(
             features::kAutofillEnableEmailHeuristicOutsideForms)) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableEmailHeuristicOnlyAddressForms);

    if (parse_standalone_email_fields) {
      FormFieldParser::ParseStandaloneEmailFields(context, fields_,
                                                  field_type_map);
    }
  }
  return field_type_map;
}

void FormStructure::AssignBestFieldTypes(
    const FieldCandidatesMap& field_type_map,
    HeuristicSource heuristic_source) {
  if (field_type_map.empty()) {
    return;
  }

  // Fields can share the same field signature. This map records for each
  // signature how many fields with the same signature have been observed.
  auto field_rank_map = base::MakeFlatMap<FieldSignature, size_t>(
      fields_, std::less<>(), [](const std::unique_ptr<AutofillField>& field) {
        return std::make_pair(field->GetFieldSignature(), 0);
      });
  for (const auto& field : fields_) {
    auto iter = field_type_map.find(field->global_id());
    if (iter == field_type_map.end())
      continue;

    const FieldCandidates& candidates = iter->second;
    field->set_heuristic_type(heuristic_source, candidates.BestHeuristicType());

    const size_t field_rank = ++field_rank_map.at(field->GetFieldSignature());
    // Log the field type predicted from local heuristics.
    field->AppendLogEventIfNotRepeated(HeuristicPredictionFieldLogEvent{
        .field_type = field->heuristic_type(heuristic_source),
        .heuristic_source = heuristic_source,
        .is_active_heuristic_source =
            GetActiveHeuristicSource() == heuristic_source,
        .rank_in_field_signature_group = field_rank,
    });
  }
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  return fields_[index].get();
}

AutofillField* FormStructure::field(size_t index) {
  return const_cast<AutofillField*>(std::as_const(*this).field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

const AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) const {
  auto it = base::ranges::find(
      fields_, field_id, [](const auto& field) { return field->global_id(); });
  return it != fields_.end() ? it->get() : nullptr;
}

AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) {
  return const_cast<AutofillField*>(
      std::as_const(*this).GetFieldById(field_id));
}

size_t FormStructure::active_field_count() const {
  return active_field_count_;
}

bool FormStructure::is_form_element() const {
  return !renderer_id_.is_null() ||
         (!fields_.empty() &&
          fields_.begin()->get()->form_control_type() ==
              FormControlType::kContentEditable &&
          *fields_.begin()->get()->renderer_id() == *renderer_id_);
}

FormData FormStructure::ToFormData() const {
  FormData data;
  data.set_id_attribute(id_attribute_);
  data.set_name_attribute(name_attribute_);
  data.set_name(form_name_);
  data.set_button_titles(button_titles_);
  data.set_url(source_url_);
  data.set_full_url(full_source_url_);
  data.set_action(target_url_);
  data.set_main_frame_origin(main_frame_origin_);
  data.set_renderer_id(renderer_id_);
  data.set_host_frame(host_frame_);
  data.set_version(version_);
  data.set_child_frames(child_frames_);
  std::vector<FormFieldData> fields;
  fields.reserve(fields_.size());
  for (const auto& field : fields_) {
    fields.push_back(*field);
  }
  data.set_fields(std::move(fields));
  return data;
}

void FormStructure::ProcessExtractedFields() {
  // Extracts the |parseable_name_| by removing common affixes from the
  // field names.
  ExtractParseableFieldNames();

  // TODO(crbug.com/40741721): Remove once shared labels are launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForParsingWithSharedLabels)) {
    // Extracts the |parsable_label_| for each field.
    ExtractParseableFieldLabels();
  }
}

void FormStructure::ExtractParseableFieldLabels() {
  std::vector<std::u16string_view> field_labels;
  field_labels.reserve(field_count());
  for (const auto& field : *this) {
    // Skip fields that are not a text input or not visible.
    if (!field->IsTextInputElement() || !field->IsFocusable()) {
      continue;
    }
    field_labels.push_back(field->label());
  }

  // Determine the parsable labels and write them back.
  std::optional<std::vector<std::u16string>> parsable_labels =
      GetParseableLabels(field_labels);
  // If not single label was split, the function can return, because the
  // |parsable_label_| is assigned to |label| by default.
  if (!parsable_labels.has_value()) {
    return;
  }

  size_t idx = 0;
  for (auto& field : *this) {
    if (!field->IsTextInputElement() || !field->IsFocusable()) {
      // For those fields, set the original label.
      field->set_parseable_label(field->label());
      continue;
    }
    DCHECK(idx < parsable_labels->size());
    field->set_parseable_label(parsable_labels->at(idx++));
  }
}

void FormStructure::ExtractParseableFieldNames() {
  std::vector<std::u16string_view> names;
  names.reserve(field_count());
  for (const auto& field : *this)
    names.emplace_back(field->name());

  // Determine the parseable names and write them into the corresponding field.
  ComputeParseableNames(names);
  size_t idx = 0;
  for (auto& field : *this)
    field->set_parseable_name(std::u16string(names[idx++]));
}

DenseSet<FormType> FormStructure::GetFormTypes() const {
  DenseSet<FormType> form_types;
  for (const auto& field : fields_) {
    if (field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      // When `kAutofillPredictionsForAutocompleteUnrecognized` is enabled,
      // types are predicted for fields with unrecognized autocomplete
      // attribute. They are excluded from the form types, to keep the baseline
      // for key and quality metrics.
      form_types.insert(FormType::kUnknownFormType);
    } else {
      form_types.insert(FieldTypeGroupToFormType(field->Type().group()));
    }
  }
  return form_types;
}

void FormStructure::set_randomized_encoder(
    std::unique_ptr<RandomizedEncoder> encoder) {
  randomized_encoder_ = std::move(encoder);
}

void FormStructure::RationalizePhoneNumbersInSection(const Section& section) {
  if (base::Contains(phone_rationalized_, section))
    return;
  FormStructureRationalizer rationalizer(&fields_);
  rationalizer.RationalizePhoneNumbersInSection(section);
  phone_rationalized_.insert(section);
}

void FormStructure::RationalizeFormStructure(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  FormStructureRationalizer rationalizer(&fields_);
  rationalizer.RationalizeContentEditables(log_manager);
  rationalizer.RationalizeAutocompleteAttributes(log_manager);
  rationalizer.RationalizeRepeatedFields(
      form_signature(), form_interactions_ukm_logger, log_manager);
  rationalizer.RationalizeFieldTypePredictions(
      main_frame_origin(), client_country(), current_page_language(),
      log_manager);
}

std::ostream& operator<<(std::ostream& buffer, const FormStructure& form) {
  buffer << "\nForm signature: "
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << "\n Form name: " << form.form_name();
  buffer << "\n Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << "\n Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << "\n Field " << i << ": ";
    const AutofillField* field = form.field(i);
    buffer << "\n  Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->renderer_id().value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin().Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id().value())});
    buffer << "\n  Signature: "
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature().value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature()))});
    buffer << "\n  Name: " << field->parseable_name();

    auto type = field->Type().ToStringView();
    auto heuristic_type = FieldTypeToStringView(field->heuristic_type());
    std::string server_type = ServerTypesToString(field);
    const char* is_override =
        field->server_type_prediction_is_override() ? " (manual override)" : "";
    auto html_type_description =
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringView(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
      html_type_description += " (disabling autofill)";
    }

    buffer << "\n  Type: "
           << base::StrCat({type, " (heuristic: ", heuristic_type,
                            ", server: ", server_type, is_override,
                            html_type_description, ")"});
    buffer << "\n  Section: " << field->section();

    constexpr size_t kMaxLabelSize = 100;
    const std::u16string truncated_label = field->label().substr(
        0, std::min(field->label().length(), kMaxLabelSize));
    buffer << "\n  Label: " << truncated_label;

    buffer << "\n  Is empty: "
           << (field->value(ValueSemantics::kCurrent).empty() ? "Yes" : "No");
  }
  return buffer;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form) {
  buffer << Tag{"div"} << Attrib{"class", "form"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Form signature:"
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << Tr{} << "Form alternative signature:"
         << base::StrCat({base::NumberToString(
                              form.alternative_form_signature().value()),
                          " - ",
                          base::NumberToString(HashFormSignature(
                              form.alternative_form_signature()))});
  buffer << Tr{} << "Form name:" << form.form_name();
  buffer << Tr{} << "Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << Tr{} << "Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    const AutofillField* field = form.field(i);
    buffer << Tag{"td"};
    buffer << Tag{"table"};
    buffer << Tr{} << "Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->renderer_id().value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin().Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id().value())});
    buffer << Tr{} << "Signature:"
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature().value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature()))});
    buffer << Tr{} << "Name:" << field->parseable_name();
    buffer << Tr{} << "Placeholder:" << field->placeholder();

    auto type = field->Type().ToStringView();
    auto heuristic_type = FieldTypeToStringView(field->heuristic_type());
    std::string server_type = ServerTypesToString(field);
    if (field->server_type_prediction_is_override())
      server_type += " (manual override)";
    auto html_type_description =
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringView(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
      html_type_description += " (disabling autofill)";
    }

    buffer << Tr{} << "Type:"
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << Tr{} << "Section:" << field->section();

    constexpr size_t kMaxLabelSize = 100;
    // TODO(crbug.com/40741721): Remove once shared labels are launched.
    const std::u16string& label =
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForParsingWithSharedLabels)
            ? field->parseable_label()
            : field->label();
    const std::u16string truncated_label =
        label.substr(0, std::min(label.length(), kMaxLabelSize));
    buffer << Tr{} << "Label:" << truncated_label;

    buffer << Tr{} << "Is empty:"
           << (field->value(ValueSemantics::kCurrent).empty() ? "Yes" : "No");
    buffer << Tr{} << "Is focusable:"
           << (field->IsFocusable() ? "Yes (focusable)" : "No (unfocusable)");
    buffer << Tr{} << "Is visible:"
           << (field->is_visible() ? "Yes (visible)" : "No (invisible)");
    buffer << Tr{} << "Ranks: "
           << base::StringPrintf(
                  "Field rank: %zu, rank in signature group: %zu, "
                  "field rank in host form: %zu, rank in host form signature "
                  "group: %zu",
                  field->rank(), field->rank_in_signature_group(),
                  field->rank_in_host_form(),
                  field->rank_in_host_form_signature_group());
    if (field->may_use_prefilled_placeholder().has_value()) {
      buffer << Tr{} << "Pre-filled value:"
             << base::StrCat(
                    {"is classified as ",
                     (*field->may_use_prefilled_placeholder() ? "a placeholder"
                                                              : "meaningful")});
    }
    buffer << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

FormDataAndServerPredictions::FormDataAndServerPredictions() = default;

FormDataAndServerPredictions::FormDataAndServerPredictions(
    const FormDataAndServerPredictions&) = default;

FormDataAndServerPredictions& FormDataAndServerPredictions::operator=(
    const FormDataAndServerPredictions&) = default;

FormDataAndServerPredictions::FormDataAndServerPredictions(
    FormDataAndServerPredictions&&) = default;

FormDataAndServerPredictions& FormDataAndServerPredictions::operator=(
    FormDataAndServerPredictions&&) = default;

FormDataAndServerPredictions::~FormDataAndServerPredictions() = default;

FormDataAndServerPredictions GetFormDataAndServerPredictions(
    const FormStructure& form) {
  FormDataAndServerPredictions result;
  std::vector<std::pair<FieldGlobalId, AutofillType::ServerPrediction>>
      predictions;
  result.form_data = form.ToFormData();
  predictions.reserve(form.fields().size());
  for (const std::unique_ptr<AutofillField>& field : form) {
    predictions.emplace_back(field->global_id(),
                             AutofillType::ServerPrediction(*field));
  }
  result.predictions =
      base::flat_map<FieldGlobalId, AutofillType::ServerPrediction>(
          std::move(predictions));
  return result;
}

}  // namespace autofill
