// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/standalone_cvc_field.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

// static
std::unique_ptr<FormField> StandaloneCvcField::Parse(
    AutofillScanner* scanner,
    const LanguageCode& page_language,
    PatternSource pattern_source,
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields)) {
    return nullptr;
  }

  // Ignore gift card fields as both |kGiftCardRe| and |kCardCvcRe| matches
  // "gift card pin" and "gift card code" but it should only match
  // |kGiftCardRe|.
  if (MatchGiftCard(scanner, log_manager, page_language, pattern_source)) {
    return nullptr;
  }

  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> cvc_patterns = GetMatchPatterns(
      CREDIT_CARD_VERIFICATION_CODE, page_language, pattern_source);

  // CVC fields can occur in many different field types so we check for each
  const auto kMatchNumTelAndPwd =
      kDefaultMatchParamsWith<MatchFieldType::kNumber,
                              MatchFieldType::kTelephone,
                              MatchFieldType::kPassword>;
  if (ParseFieldSpecifics(scanner, kCardCvcRe, kMatchNumTelAndPwd, cvc_patterns,
                          &field, {log_manager, "kCardCvcRe(standalone)"})) {
    return std::make_unique<StandaloneCvcField>(field);
  }

  return nullptr;
}

StandaloneCvcField::~StandaloneCvcField() = default;

// static
bool StandaloneCvcField::MatchGiftCard(AutofillScanner* scanner,
                                       LogManager* log_manager,
                                       const LanguageCode& page_language,
                                       PatternSource pattern_source) {
  if (scanner->IsEnd())
    return false;

  const auto kMatchFieldType = kDefaultMatchParamsWith<
      MatchFieldType::kNumber, MatchFieldType::kTelephone,
      MatchFieldType::kSearch, MatchFieldType::kPassword>;
  base::span<const MatchPatternRef> gift_card_patterns =
      GetMatchPatterns("GIFT_CARD", page_language, pattern_source);

  size_t saved_cursor = scanner->SaveCursor();
  const bool gift_card_match = ParseFieldSpecifics(
      scanner, kGiftCardRe, kMatchFieldType, gift_card_patterns, nullptr,
      {log_manager, "kGiftCardRe"});
  // MatchGiftCard only wants to test the presence of a gift card but not
  // consume the field.
  scanner->RewindTo(saved_cursor);

  return gift_card_match;
}

StandaloneCvcField::StandaloneCvcField(const AutofillField* field)
    : field_(field) {}

void StandaloneCvcField::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
                    kBaseCreditCardParserScore, field_candidates);
}

}  // namespace autofill
