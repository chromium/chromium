// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/quick_answers_model.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

double MaybeGetRatio(double value1, double value2) {
  if (value1 == quick_answers::kInvalidRateTermValue ||
      value2 == quick_answers::kInvalidRateTermValue) {
    return quick_answers::kInvalidRateTermValue;
  }

  return std::max(value1, value2) / std::min(value1, value2);
}

int GetFormulaMessageId(bool is_multiply, bool is_approximate) {
  if (is_multiply) {
    if (is_approximate) {
      return IDS_RICH_ANSWERS_VIEW_UNIT_CONVERSION_APPROXIMATE_MULTIPLICATION_FORMULA_TEXT;
    }

    return IDS_RICH_ANSWERS_VIEW_UNIT_CONVERSION_EXACT_MULTIPLICATION_FORMULA_TEXT;
  }

  if (is_approximate) {
    return IDS_RICH_ANSWERS_VIEW_UNIT_CONVERSION_APPROXIMATE_DIVISION_FORMULA_TEXT;
  }

  return IDS_RICH_ANSWERS_VIEW_UNIT_CONVERSION_EXACT_DIVISION_FORMULA_TEXT;
}

}  // namespace

namespace quick_answers {

std::optional<quick_answers::Intent> ToIntent(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kDictionary:
      return quick_answers::Intent::kDefinition;
    case IntentType::kTranslation:
      return quick_answers::Intent::kTranslation;
    case IntentType::kUnit:
      return quick_answers::Intent::kUnitConversion;
    case IntentType::kUnknown:
      return std::nullopt;
  }

  CHECK(false) << "Invalid intent type enum value provided";
}

PhoneticsInfo::PhoneticsInfo() = default;
PhoneticsInfo::PhoneticsInfo(const PhoneticsInfo&) = default;
PhoneticsInfo::~PhoneticsInfo() = default;

bool PhoneticsInfo::PhoneticsInfoAvailable() const {
  return AudioUrlAvailable() || TtsAudioAvailable();
}

bool PhoneticsInfo::AudioUrlAvailable() const {
  return !phonetics_audio.is_empty();
}

bool PhoneticsInfo::TtsAudioAvailable() const {
  if (!tts_audio_enabled) {
    return false;
  }

  return !query_text.empty() && !locale.empty();
}

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

  // If neither |term_a| nor |term_c| is valid, there is no valid conversion
  // rule.
  if ((!term_a || term_a.value() == kInvalidRateTermValue) &&
      (!term_c || term_c.value() == kInvalidRateTermValue)) {
    return std::nullopt;
  }

  // Neither |term_a| nor |term_c| should be negative. Return nullopt for this
  // unexpected case.
  if ((term_a && term_a.value() < 0) || (term_c && term_c.value() < 0)) {
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
bool ConversionRule::IsSingleVariableLinearConversion() const {
  return (term_a_ != kInvalidRateTermValue) &&
         (term_b_ == kInvalidRateTermValue) &&
         (term_c_ == kInvalidRateTermValue);
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
      MaybeGetRatio(source_rule_.term_a(), dest_rule_.term_a());
  if (linear_term_ratio == kInvalidRateTermValue) {
    return false;
  }

  double other_linear_term_ratio =
      MaybeGetRatio(other.source_rule_.term_a(), other.dest_rule_.term_a());
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
  // We only return formula description texts for conversions between two units
  // whose `ConversionRule` only involves |term_a_| values i.e. formula form is:
  // a * source = target.
  if (!source_rule_.IsSingleVariableLinearConversion() ||
      !dest_rule_.IsSingleVariableLinearConversion()) {
    return std::nullopt;
  }

  // Don't return a formula description text if the conversion rate is
  // exactly 1.
  if (source_rule_.term_a() == dest_rule_.term_a()) {
    return std::nullopt;
  }

  // Get the greater ratio (i.e. >= 1) of the two linear |term_a_| values.
  // The conversion formula will be in the form: source * (a1/a2) = target
  // The actual ratio will determine whether the conversion operator used for
  // the formula description is multiplication (when the source unit |term_a|
  // value is the numerator) or division (when the target unit |term_a| value
  // is the numerator).
  double conversion_term_a =
      MaybeGetRatio(source_rule_.term_a(), dest_rule_.term_a());

  if (conversion_term_a == kInvalidRateTermValue) {
    return std::nullopt;
  }

  // Check if the conversion term is a decimal number. If it is, an
  // approximation qualifier (i.e. "for an approximate result, ...") will be
  // appended at the beginning of the formula text.
  double int_part = 0.0;
  bool is_approximate_formula = std::modf(conversion_term_a, &int_part) != 0.0;

  bool is_multiply_formula = source_rule_.term_a() > dest_rule_.term_a();
  int formula_message_id =
      GetFormulaMessageId(is_multiply_formula, is_approximate_formula);

  return l10n_util::GetStringFUTF8(
      formula_message_id,
      base::UTF8ToUTF16(base::ToLowerASCII(source_rule_.category())),
      base::UTF8ToUTF16(BuildRoundedUnitAmountDisplayText(conversion_term_a)));
}

UnitConversionResult::UnitConversionResult() = default;
UnitConversionResult::UnitConversionResult(const UnitConversionResult& other) =
    default;
UnitConversionResult& UnitConversionResult::UnitConversionResult::operator=(
    const UnitConversionResult& other) = default;
UnitConversionResult::~UnitConversionResult() = default;

StructuredResult::StructuredResult() = default;
StructuredResult::~StructuredResult() = default;
ResultType StructuredResult::GetResultType() const {
  if (translation_result) {
    return ResultType::kTranslationResult;
  }
  if (definition_result) {
    return ResultType::kDefinitionResult;
  }
  if (unit_conversion_result) {
    return ResultType::kUnitConversionResult;
  }
  return ResultType::kNoResult;
}

QuickAnswersSession::QuickAnswersSession() = default;
QuickAnswersSession::~QuickAnswersSession() = default;

}  // namespace quick_answers
