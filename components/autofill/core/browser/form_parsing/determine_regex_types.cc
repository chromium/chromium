// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/prediction_quality_metrics.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

namespace {

// Classifies each field using the regular expressions. The classifications
// are returned, but not assigned to the `fields_` yet. Use
// `AssignBestFieldTypes()` to do so.
FieldCandidatesMap ParseFieldTypesWithPatterns(const FormData& form,
                                               ParsingContext& context) {
  FieldCandidatesMap field_type_map;

  if (ShouldRunHeuristics(form)) {
    FormFieldParser::ParseFormFields(context, form.fields(), field_type_map);
  } else if (ShouldRunHeuristicsForSingleFields(form)) {
    FormFieldParser::ParseSingleFields(context, form.fields(), field_type_map);
    FormFieldParser::ParseStandaloneCVCFields(context, form.fields(),
                                              field_type_map);

    // For standalone email fields, allow heuristics even when the minimum
    // number of fields is not met. See similar comments in
    // `FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields`.
    FormFieldParser::ParseStandaloneEmailFields(context, form.fields(),
                                                field_type_map);

    // Try parsing standalone loyalty card fields after an attempt has been
    // made to parse multi-purpose input fields e.g. email or loyalty number
    // fields.
    FormFieldParser::ParseStandaloneLoyaltyCardFields(context, form.fields(),
                                                      field_type_map);
  }
  return field_type_map;
}

}  // namespace

RegexPredictions::RegexPredictions(HeuristicSource source,
                                   const FieldCandidatesMap& field_type_map,
                                   base::span<const FormFieldData> fields)
    : source_(source) {
  const HeuristicSource active_source = GetActiveHeuristicSource();
  std::vector<std::pair<FieldGlobalId, FieldType>> field_predictions;
  for (const FormFieldData& field : fields) {
    auto iter = field_type_map.find(field.global_id());
    if (iter == field_type_map.end()) {
      continue;
    }

    const FieldCandidates& candidates = iter->second;
    if (source_ == active_source) {
      autofill_metrics::LogLocalHeuristicMatchedAttribute(
          candidates.BestHeuristicTypeReason());
    }

    field_predictions.emplace_back(field.global_id(),
                                   candidates.BestHeuristicType());
  }
  predictions_ = base::flat_map(std::move(field_predictions));
}

RegexPredictions::RegexPredictions(const RegexPredictions&) = default;

RegexPredictions::RegexPredictions(RegexPredictions&&) = default;

RegexPredictions& RegexPredictions::operator=(const RegexPredictions&) =
    default;

RegexPredictions& RegexPredictions::operator=(RegexPredictions&&) = default;

RegexPredictions::~RegexPredictions() = default;

void RegexPredictions::ApplyTo(
    base::span<const std::unique_ptr<AutofillField>> fields) const {
  // Fields can share the same field signature. This map records for each
  // signature how many fields with the same signature have been observed.
  auto field_rank_map = base::MakeFlatMap<FieldSignature, size_t>(
      fields, std::less<>(), [](const std::unique_ptr<AutofillField>& field) {
        return std::make_pair(field->GetFieldSignature(), 0);
      });

  for (const std::unique_ptr<AutofillField>& field : fields) {
    auto it = predictions_.find(field->global_id());
    if (it == predictions_.end()) {
      continue;
    }
    field->set_heuristic_type(source_, it->second);

    const size_t field_rank = ++field_rank_map.at(field->GetFieldSignature());
    // Log the field type predicted from local heuristics.
    field->AppendLogEventIfNotRepeated(HeuristicPredictionFieldLogEvent{
        .field_type = field->heuristic_type(source_),
        .heuristic_source = source_,
        .is_active_heuristic_source = GetActiveHeuristicSource() == source_,
        .rank_in_field_signature_group = field_rank,
    });
  }
}

RegexPredictions DetermineRegexTypes(const GeoIpCountryCode& client_country,
                                     const LanguageCode& current_page_language,
                                     const FormData& form,
                                     LogManager* log_manager) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DetermineHeuristicTypes");

  const LanguageCode& page_language =
      base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)
          ? current_page_language
          : LanguageCode();
  ParsingContext context(form.fields(), client_country, page_language,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                         PatternFile::kDefault,
#else
                         PatternFile::kLegacy,
#endif
                         GetActiveRegexFeatures(), log_manager);
  return RegexPredictions(HeuristicSource::kRegexes,
                          ParseFieldTypesWithPatterns(form, context),
                          form.fields());
}

}  // namespace autofill
