// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

namespace {

double GetRatio(double value1, double value2) {
  if (value1 == quick_answers::kInvalidRateTermValue ||
      value2 == quick_answers::kInvalidRateTermValue) {
    return quick_answers::kInvalidRateTermValue;
  }

  return std::max(value1, value2) / std::min(value1, value2);
}

}  // namespace

namespace quick_answers {

PhoneticsInfo::PhoneticsInfo() = default;
PhoneticsInfo::PhoneticsInfo(const PhoneticsInfo&) = default;
PhoneticsInfo::~PhoneticsInfo() = default;

QuickAnswer::QuickAnswer() = default;
QuickAnswer::~QuickAnswer() = default;

IntentInfo::IntentInfo() = default;
IntentInfo::IntentInfo(const IntentInfo& other) = default;
IntentInfo::IntentInfo(const std::string& intent_text,
                       IntentType intent_type,
                       const std::string& device_language,
                       const std::string& source_language) {
  this->intent_text = intent_text;
  this->intent_type = intent_type;
  this->device_language = device_language;
  this->source_language = source_language;
}
IntentInfo::~IntentInfo() = default;

PreprocessedOutput::PreprocessedOutput() = default;
PreprocessedOutput::PreprocessedOutput(const PreprocessedOutput& other) =
    default;
PreprocessedOutput::~PreprocessedOutput() = default;

QuickAnswersRequest::QuickAnswersRequest() = default;
QuickAnswersRequest::QuickAnswersRequest(const QuickAnswersRequest& other) =
    default;
QuickAnswersRequest::~QuickAnswersRequest() = default;

Sense::Sense() = default;
Sense::Sense(const Sense& other) = default;
Sense& Sense::Sense::operator=(const Sense& other) = default;
Sense::~Sense() = default;

DefinitionResult::DefinitionResult() = default;
DefinitionResult::DefinitionResult(const DefinitionResult& other) = default;
DefinitionResult& DefinitionResult::DefinitionResult::operator=(
    const DefinitionResult& other) = default;
DefinitionResult::~DefinitionResult() = default;

TranslationResult::TranslationResult() = default;
TranslationResult::TranslationResult(const TranslationResult& other) = default;
TranslationResult& TranslationResult::TranslationResult::operator=(
    const TranslationResult& other) = default;
TranslationResult::~TranslationResult() = default;

ConversionRule::ConversionRule(const std::string& category,
                               const std::string& unit_name,
                               double term_a,
                               double term_b,
                               double term_c)
    : category_(category),
      unit_name_(unit_name),
      term_a_(term_a),
      term_b_(term_b),
      term_c_(term_c) {}
ConversionRule::ConversionRule(const ConversionRule& other) = default;
ConversionRule& ConversionRule::ConversionRule::operator=(
    const ConversionRule& other) = default;
ConversionRule::~ConversionRule() = default;
std::optional<ConversionRule> ConversionRule::Create(
    const std::string& category,
    const std::string& unit_name,
    const std::optional<double>& term_a,
    const std::optional<double>& term_b,
    const std::optional<double>& term_c) {
  if (category.empty() || unit_name.empty()) {
    return std::nullopt;
  }

  // If neither term_a nor term_c is valid, there is no valid conversion rule.
  if ((!term_a || term_a.value() == kInvalidRateTermValue) &&
      (!term_c || term_c.value() == kInvalidRateTermValue)) {
    return std::nullopt;
  }

  double term_a_value = term_a.value_or(kInvalidRateTermValue);
  double term_b_value = term_b.value_or(kInvalidRateTermValue);
  double term_c_value = term_c.value_or(kInvalidRateTermValue);
  return ConversionRule(category, unit_name, term_a_value, term_b_value,
                        term_c_value);
}
double ConversionRule::ConvertAmountToSi(double unit_amount) const {
  return (term_a_ != kInvalidRateTermValue) ? (term_a_ * unit_amount + term_b_)
                                            : (term_c_ / unit_amount);
}
double ConversionRule::ConvertAmountFromSi(double si_amount) const {
  return (term_a_ != kInvalidRateTermValue) ? ((si_amount - term_b_) / term_a_)
                                            : (term_c_ / si_amount);
}

UnitConversion::UnitConversion(const ConversionRule& source_rule,
                               const ConversionRule& dest_rule)
    : source_rule_(source_rule), dest_rule_(dest_rule) {}
UnitConversion::UnitConversion(const UnitConversion& other) = default;
UnitConversion& UnitConversion::UnitConversion::operator=(
    const UnitConversion& other) = default;
UnitConversion::~UnitConversion() = default;
std::optional<UnitConversion> UnitConversion::Create(
    const ConversionRule& source_rule,
    const ConversionRule& dest_rule) {
  if (source_rule.category() != dest_rule.category()) {
    return std::nullopt;
  }

  return UnitConversion(source_rule, dest_rule);
}
bool UnitConversion::operator<(const UnitConversion& other) const {
  double linear_term_ratio =
      GetRatio(source_rule_.linear_term(), dest_rule_.linear_term());
  if (linear_term_ratio == kInvalidRateTermValue) {
    return false;
  }

  double other_linear_term_ratio = GetRatio(other.source_rule_.linear_term(),
                                            other.dest_rule_.linear_term());
  if (other_linear_term_ratio == kInvalidRateTermValue) {
    return true;
  }

  return linear_term_ratio < other_linear_term_ratio;
}
double UnitConversion::ConvertSourceAmountToDestAmount(
    double source_amount) const {
  return dest_rule_.ConvertAmountFromSi(
      source_rule_.ConvertAmountToSi(source_amount));
}
std::optional<std::string> UnitConversion::GetConversionFormulaText() const {
  // TODO(b/322418683): implement this function builder
  return std::nullopt;
}

UnitConversionResult::UnitConversionResult() = default;
UnitConversionResult::UnitConversionResult(const UnitConversionResult& other) =
    default;
UnitConversionResult& UnitConversionResult::UnitConversionResult::operator=(
    const UnitConversionResult& other) = default;
UnitConversionResult::~UnitConversionResult() = default;

StructuredResult::StructuredResult() = default;
StructuredResult::~StructuredResult() = default;

QuickAnswersSession::QuickAnswersSession() = default;
QuickAnswersSession::~QuickAnswersSession() = default;

}  // namespace quick_answers
