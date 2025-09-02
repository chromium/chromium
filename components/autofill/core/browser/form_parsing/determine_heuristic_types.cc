// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/determine_heuristic_types.h"

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

namespace {

raw_ptr<const FormFieldData> to_form_field_data(
    const std::unique_ptr<AutofillField>& field) {
  return field.get();
}

// Classifies each field using the regular expressions. The classifications
// are returned, but not assigned to the `fields_` yet. Use
// `AssignBestFieldTypes()` to do so.
FieldCandidatesMap ParseFieldTypesWithPatterns(FormStructure& form,
                                               ParsingContext& context) {
  FieldCandidatesMap field_type_map;

  auto form_field_data_vector = base::ToVector(
      form.fields(),
      [](const auto& f) -> raw_ptr<const FormFieldData> { return f.get(); });
  if (form.ShouldRunHeuristics()) {
    FormFieldParser::ParseFormFields(context, form_field_data_vector,
                                     field_type_map);
  } else if (form.ShouldRunHeuristicsForSingleFields()) {
    FormFieldParser::ParseSingleFields(context, form_field_data_vector,
                                       field_type_map);
    FormFieldParser::ParseStandaloneCVCFields(context, form_field_data_vector,
                                              field_type_map);

    // For standalone email fields, allow heuristics even when the minimum
    // number of fields is not met. See similar comments in
    // `FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields`.
    FormFieldParser::ParseStandaloneEmailFields(context, form_field_data_vector,
                                                field_type_map);

    // Try parsing standalone loyalty card fields after an attempt has been
    // made to parse multi-purpose input fields e.g. email or loyalty number
    // fields.
    FormFieldParser::ParseStandaloneLoyaltyCardFields(
        context, form_field_data_vector, field_type_map);
  }
  return field_type_map;
}

// Assigns the best heuristic types from the `field_type_map` to the heuristic
// types of the corresponding fields for the `pattern_source`.
void AssignBestFieldTypes(
    const FieldCandidatesMap& field_type_map,
    HeuristicSource heuristic_source,
    base::span<const std::unique_ptr<AutofillField>> fields) {
  if (field_type_map.empty()) {
    return;
  }

  // Fields can share the same field signature. This map records for each
  // signature how many fields with the same signature have been observed.
  auto field_rank_map = base::MakeFlatMap<FieldSignature, size_t>(
      fields, std::less<>(), [](const std::unique_ptr<AutofillField>& field) {
        return std::make_pair(field->GetFieldSignature(), 0);
      });
  for (const auto& field : fields) {
    auto iter = field_type_map.find(field->global_id());
    if (iter == field_type_map.end()) {
      continue;
    }

    const FieldCandidates& candidates = iter->second;
    field->set_heuristic_type(heuristic_source, candidates.BestHeuristicType());
    if (heuristic_source == GetActiveHeuristicSource()) {
      autofill_metrics::LogLocalHeuristicMatchedAttribute(
          candidates.BestHeuristicTypeReason());
    }

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

}  // namespace

void DetermineHeuristicTypes(const GeoIpCountryCode& client_country,
                             const LanguageCode& current_page_language,
                             FormStructure& form,
                             LogManager* log_manager) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DetermineHeuristicTypes");

  const LanguageCode& page_language =
      base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)
          ? current_page_language
          : LanguageCode();
  ParsingContext context(base::ToVector(form.fields(), &to_form_field_data),
                         client_country, page_language,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                         PatternFile::kDefault,
#else
                         PatternFile::kLegacy,
#endif
                         GetActiveRegexFeatures(), log_manager);
  FieldCandidatesMap regex_predictions =
      ParseFieldTypesWithPatterns(form, context);
  AssignBestFieldTypes(regex_predictions, HeuristicSource::kRegexes,
                       form.fields());
}

}  // namespace autofill
