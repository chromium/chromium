// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/standalone_cvc_field_parser.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

// static
std::unique_ptr<FormFieldParser> StandaloneCvcFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields)) {
    return nullptr;
  }

  // Ignore gift card fields as both |kGiftCardRe| and |kCardCvcRe| matches
  // "gift card pin" and "gift card code" but it should only match
  // |kGiftCardRe|.
  if (MatchGiftCard(context, scanner)) {
    return nullptr;
  }

  raw_ptr<AutofillField> field;
  base::span<const MatchPatternRef> cvc_patterns =
      GetMatchPatterns(CREDIT_CARD_VERIFICATION_CODE, context.page_language,
                       context.pattern_file);

  if (ParseField(context, scanner, cvc_patterns, &field,
                 "CREDIT_CARD_VERIFICATION_CODE(standalone)")) {
    return std::make_unique<StandaloneCvcFieldParser>(field);
  }

  return nullptr;
}

StandaloneCvcFieldParser::~StandaloneCvcFieldParser() = default;

// static
bool StandaloneCvcFieldParser::MatchGiftCard(ParsingContext& context,
                                             AutofillScanner* scanner) {
  if (scanner->IsEnd()) {
    return false;
  }

  base::span<const MatchPatternRef> gift_card_patterns = GetMatchPatterns(
      "GIFT_CARD", context.page_language, context.pattern_file);

  size_t saved_cursor = scanner->SaveCursor();
  const bool gift_card_match =
      ParseField(context, scanner, gift_card_patterns, nullptr, "GIFT_CARD");
  // MatchGiftCard only wants to test the presence of a gift card but not
  // consume the field.
  scanner->RewindTo(saved_cursor);

  return gift_card_match;
}

StandaloneCvcFieldParser::StandaloneCvcFieldParser(const AutofillField* field)
    : field_(field) {}

void StandaloneCvcFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
  AddClassification(field_, CREDIT_CARD_STANDALONE_VERIFICATION_CODE,
                    kBaseCreditCardParserScore, field_candidates);
}

}  // namespace autofill
