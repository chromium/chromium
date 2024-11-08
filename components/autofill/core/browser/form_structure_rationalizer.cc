// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalizer.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/credit_card_field_parser.h"
#include "components/autofill/core/browser/form_structure_rationalization_engine.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

namespace {

void RationalizePhoneNumbersForFilling(std::vector<AutofillField*>& fields) {
  // A whole phone number can be structured in the following ways:
  // - whole number
  // - country code, city and number
  // - country code, city code, number field
  // - country code, city code, number field, second number field
  // In this function more or less anything ending in a local number field
  // (see `phone_number_found` below) is accepted as a valid phone number. Any
  // phone number fields after that number are labeled as
  // set_only_fill_when_focused(true) so that they don't get filled.
  AutofillField* found_number_field = nullptr;
  AutofillField* found_number_field_second = nullptr;
  AutofillField* found_city_code_field = nullptr;
  AutofillField* found_country_code_field = nullptr;
  AutofillField* found_city_and_number_field = nullptr;
  AutofillField* found_whole_number_field = nullptr;
  // The "number" here refers to the local part of a phone number (i.e.,
  // the part after a country code and a city code). It can be found as a
  // dedicated field or as part of a bigger scope (e.g. a whole number
  // field contains a "number"). The naming is sad but a relict from the past.
  bool phone_number_found = false;
  // Whether the number field (i.e. the local part) is split into two pieces.
  // This can be observed in the US, where 650 234-5678 would be a phone
  // number whose local parts are 234 and 5678.
  bool phone_number_separate_fields = false;
  // Iterate through all given fields. Iteration stops when it first finds a
  // field that indicates the end of a phone number (this can be the local
  // part of a phone number or a whole number). The |found_*| pointers will be
  // set to that set of fields when iteration finishes.
  for (AutofillField* field : fields) {
    if (!field->is_visible()) {
      continue;
    }
    // This phone number rationalization marks all but the first phone number as
    // `set_only_fill_when_focused(true)`. Since it doesn't change the types, it
    // intentionally uses the rationalized `Type()` (over the `ComputedType()`).
    FieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
        found_number_field = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_NUMBER_PREFIX:
        if (!found_number_field) {
          found_number_field = field;
          // phone_number_found is not set to true because the suffix needs to
          // be found first.
          phone_number_separate_fields = true;
        }
        break;
      case PHONE_HOME_NUMBER_SUFFIX:
        if (phone_number_separate_fields) {
          found_number_field_second = field;
          phone_number_found = true;
        }
        break;
      case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      case PHONE_HOME_CITY_CODE:
        if (!found_city_code_field) {
          found_city_code_field = field;
        }
        break;
      case PHONE_HOME_COUNTRY_CODE:
        if (!found_country_code_field) {
          found_country_code_field = field;
        }
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
        DCHECK(!phone_number_found && !found_city_and_number_field);
        found_city_and_number_field = field;
        phone_number_found = true;
        break;
      case PHONE_HOME_WHOLE_NUMBER:
        DCHECK(!phone_number_found && !found_whole_number_field);
        found_whole_number_field = field;
        phone_number_found = true;
        break;
      default:
        break;
    }
    // Break here if the local part of a phone number was found because we
    // assume an order over the fields, where the local part comes last.
    if (phone_number_found) {
      break;
    }
  }

  // The first number of found may be the whole number field, the
  // city and number field, or neither. But it cannot be both.
  DCHECK(!(found_whole_number_field && found_city_and_number_field));

  // Prefer to fill the first complete phone number found. The whole_number
  // and city_and_number fields are only set if they occur before the local
  // part of a phone number. If we see the local part of a complete phone
  // number, we assume that the complete phone number is represented as a
  // sequence of fields (country code, city code, local part). These scenarios
  // are mutually exclusive, so clean up any inconsistencies.
  if (found_whole_number_field) {
    found_number_field = nullptr;
    found_number_field_second = nullptr;
    found_city_code_field = nullptr;
    found_country_code_field = nullptr;
  } else if (found_city_and_number_field) {
    found_number_field = nullptr;
    found_number_field_second = nullptr;
    found_city_code_field = nullptr;
  }

  // A second update pass.
  // At this point, either |phone_number_found| is false and we should do a
  // best-effort filling for the field whose types we have seen a first time.
  // Or |phone_number_found| is true and the pointers to the fields that
  // compose the first phone number are set to not-NULL. Specifically we hope
  // to find the following:
  // 1. |found_whole_number_field| is not NULL, other pointers set to NULL, or
  // 2. |found_city_and_number_field| is not NULL, |found_country_code_field|
  // is
  //    probably not NULL, and other pointers set to NULL, or
  // 3. |found_city_code_field| and |found_number_field| are not NULL,
  //    |found_country_code_field| is probably not NULL, and other pointers
  //    are NULL.
  // 4. |found_city_code_field|, |found_number_field| and
  //    |found_number_field_second| are not NULL, |found_country_code_field|
  //    is probably not NULL, and other pointers are NULL.
  //
  // We currently don't guarantee these values. E.g. it is possible that
  // |found_city_code_field| is NULL but |found_number_field| is not NULL.

  // For all above cases, in the update pass, if one field is phone
  // number related but not one of the found fields from first pass, set their
  // |only_fill_when_focused| field to true.
  for (AutofillField* field : fields) {
    // As above, using the rationalized `Type()` is intentional.
    FieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case PHONE_HOME_NUMBER:
      case PHONE_HOME_NUMBER_PREFIX:
      case PHONE_HOME_NUMBER_SUFFIX:
        if (field != found_number_field && field != found_number_field_second) {
          field->set_only_fill_when_focused(true);
        }
        break;
      case PHONE_HOME_CITY_CODE:
      case PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
        if (field != found_city_code_field) {
          field->set_only_fill_when_focused(true);
        }
        break;
      case PHONE_HOME_COUNTRY_CODE:
        if (field != found_country_code_field) {
          field->set_only_fill_when_focused(true);
        }
        break;
      case PHONE_HOME_CITY_AND_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
        if (field != found_city_and_number_field) {
          field->set_only_fill_when_focused(true);
        }
        break;
      case PHONE_HOME_WHOLE_NUMBER:
        if (field != found_whole_number_field) {
          field->set_only_fill_when_focused(true);
        }
        break;
      default:
        break;
    }
  }
}

}  // namespace

FormStructureRationalizer::FormStructureRationalizer(
    std::vector<std::unique_ptr<AutofillField>>* fields)
    : fields_(*fields) {}
FormStructureRationalizer::~FormStructureRationalizer() = default;

void FormStructureRationalizer::RationalizeAutocompleteAttributes(
    LogManager* log_manager) {
  for (const auto& field : *fields_) {
    auto set_html_type = [&field](HtmlFieldType type) {
      field->SetHtmlType(type, field->html_mode());
    };
    // Some of the following rationalization operates only on text fields.
    bool is_text_field =
        field->IsTextInputElement() ||
        field->form_control_type() == FormControlType::kTextArea;
    switch (field->html_type()) {
      case HtmlFieldType::kAdditionalName:
        if (!is_text_field) {
          continue;
        }
        if (field->max_length() == 1) {
          set_html_type(HtmlFieldType::kAdditionalNameInitial);
        }
        break;
      // We look at kCreditCardExpDate2DigitYear and
      // kCreditCardExpDate4DigitYear as well (not just kCreditCardExp which
      // is generated by the autocomplete attribute parser) because the server
      // hints can have changed (they may not have been available during the
      // first rationalization). In that case we want to rationalize again.
      case HtmlFieldType::kCreditCardExp:
      case HtmlFieldType::kCreditCardExpDate2DigitYear:
      case HtmlFieldType::kCreditCardExpDate4DigitYear:
        if (!is_text_field) {
          continue;
        }
        if (base::FeatureList::IsEnabled(
                features::kAutofillEnableExpirationDateImprovements)) {
          FieldType server_hint = field->server_type();
          FieldType forced_field_type =
              field->server_type_prediction_is_override() ? field->server_type()
                                                          : NO_SERVER_DATA;
          CreditCardFieldParser::ExpirationDateFormat format =
              CreditCardFieldParser::DetermineExpirationDateFormat(
                  *field, /*fallback_type=*/CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                  /*server_hint=*/server_hint,
                  /*forced_field_type=*/forced_field_type);
          set_html_type(format.digits_in_expiration_year == 4
                            ? HtmlFieldType::kCreditCardExpDate4DigitYear
                            : HtmlFieldType::kCreditCardExpDate2DigitYear);
        } else {
          if (field->max_length() == 5) {
            set_html_type(HtmlFieldType::kCreditCardExpDate2DigitYear);
          } else if (field->max_length() == 7) {
            set_html_type(HtmlFieldType::kCreditCardExpDate4DigitYear);
          }
        }
        break;
      case HtmlFieldType::kCreditCardExpYear:
      case HtmlFieldType::kCreditCardExp2DigitYear:
      case HtmlFieldType::kCreditCardExp4DigitYear:
        if (!is_text_field & !field->IsSelectElement()) {
          continue;
        }
        if (base::FeatureList::IsEnabled(
                features::kAutofillEnableExpirationDateImprovements)) {
          FieldType server_hint = field->server_type();
          FieldType forced_field_type =
              field->server_type_prediction_is_override() ? field->server_type()
                                                          : NO_SERVER_DATA;
          // The default for select or list elements does not really matter
          // because it's practically always chosen from the select options.
          // The default for text elements was chosen base on statistics from
          // server side classifications (go/iqwtu).
          // Keep this in sync with
          // CreditCardFieldParser::GetExpirationYearType().
          FieldType overall_type =
              CreditCardFieldParser::DetermineExpirationYearType(
                  *field,
                  /*fallback_type=*/CREDIT_CARD_EXP_4_DIGIT_YEAR,
                  /*server_hint=*/server_hint,
                  /*forced_field_type=*/forced_field_type);
          set_html_type(overall_type == CREDIT_CARD_EXP_4_DIGIT_YEAR
                            ? HtmlFieldType::kCreditCardExp4DigitYear
                            : HtmlFieldType::kCreditCardExp2DigitYear);
        } else {
          if (field->max_length() == 2) {
            set_html_type(HtmlFieldType::kCreditCardExp2DigitYear);
          } else if (field->max_length() == 4) {
            set_html_type(HtmlFieldType::kCreditCardExp4DigitYear);
          }
        }
        break;
      default:
        break;
    }
  }
}

void FormStructureRationalizer::RationalizeContentEditables(
    LogManager* log_manager) {
  for (const auto& field : *fields_) {
    if (field->form_control_type() == FormControlType::kContentEditable) {
      field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
    }
  }
}

void FormStructureRationalizer::RationalizeCreditCardFieldPredictions(
    LogManager* log_manager) {
  bool cc_first_name_found = false;
  bool cc_last_name_found = false;
  bool cc_num_found = false;
  bool cc_month_found = false;
  bool cc_year_found = false;
  bool cc_type_found = false;
  bool cc_cvc_found = false;
  bool email_address_found = false;
  size_t num_months_found = 0;
  size_t num_other_fields_found = 0;
  for (const auto& field : *fields_) {
    FieldType current_field_type = field->ComputedType().GetStorableType();
    switch (current_field_type) {
      case CREDIT_CARD_NAME_FIRST:
        cc_first_name_found = true;
        break;
      case CREDIT_CARD_NAME_LAST:
        cc_last_name_found = true;
        break;
      case CREDIT_CARD_NAME_FULL:
        cc_first_name_found = true;
        cc_last_name_found = true;
        break;
      case CREDIT_CARD_NUMBER:
        cc_num_found = true;
        break;
      case CREDIT_CARD_EXP_MONTH:
        cc_month_found = true;
        ++num_months_found;
        break;
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        cc_year_found = true;
        break;
      case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        cc_month_found = true;
        cc_year_found = true;
        ++num_months_found;
        break;
      case CREDIT_CARD_TYPE:
        cc_type_found = true;
        break;
      case CREDIT_CARD_VERIFICATION_CODE:
        cc_cvc_found = true;
        break;
      case ADDRESS_HOME_ZIP:
        // Zip/Postal code often appears as part of a Credit Card form. Do
        // not count it as a non-cc-related field.
        break;
      case EMAIL_ADDRESS:
        email_address_found = true;
        [[fallthrough]];
      default:
        ++num_other_fields_found;
    }
  }

  // A partial CC name is unlikely. Prefer to consider these profile names
  // when partial.
  bool cc_name_found = cc_first_name_found && cc_last_name_found;

  // A partial CC expiry date should not be filled. These are often confused
  // with quantity/height fields and/or generic year fields.
  bool cc_date_found = cc_month_found && cc_year_found;

  // Count the credit card related fields in the form.
  size_t num_cc_fields_found =
      static_cast<int>(cc_name_found) + static_cast<int>(cc_num_found) +
      static_cast<int>(cc_date_found) + static_cast<int>(cc_type_found) +
      static_cast<int>(cc_cvc_found);

  // Retain credit card related fields if the form has multiple fields or has
  // no unrelated fields (useful for single cc-field forms). Credit card number
  // is permitted to be alone in an otherwise unrelated form because some
  // dynamic forms reveal the remainder of the fields only after the credit
  // card number is entered and identified as a credit card by the site.
  bool keep_cc_fields =
      cc_num_found || num_cc_fields_found >= 3 || num_other_fields_found == 0;

  if (!keep_cc_fields && num_cc_fields_found > 0) {
    LOG_AF(log_manager)
        << LoggingScope::kRationalization << LogMessage::kRationalization
        << "Credit card rationalization: Did not find credit card number, did "
           "not find >= 3 credit card fields ("
        << num_cc_fields_found << "), and had non-cc fields ("
        << num_other_fields_found << ").";
  }

  // Do an update pass over the fields to rewrite the types if credit card
  // fields are not to be retained. Some special handling is given to expiry
  // dates if the full date is not found or multiple expiry date fields are
  // found. See comments inline below.
  for (auto it = fields_->begin(); it != fields_->end(); ++it) {
    auto& field = *it;
    FieldType current_field_type = field->ComputedType().GetStorableType();
    switch (current_field_type) {
      case CREDIT_CARD_NAME_FIRST:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_FIRST));
        break;
      case CREDIT_CARD_NAME_LAST:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_LAST));
        break;
      case CREDIT_CARD_NAME_FULL:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_FULL));
        break;
      case CREDIT_CARD_NUMBER:
      case CREDIT_CARD_TYPE:
      case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        break;
      case CREDIT_CARD_EXP_MONTH:
        // Do not preserve an expiry month prediction if any of the following
        // are true:
        //   (1) the form is determined to be be non-cc related, so all cc
        //       field predictions are to be discarded
        //   (2) the expiry month was found without a corresponding year
        //   (3) multiple month fields were found in a form having a full
        //       expiry date. This usually means the form is a checkout form
        //       that also has one or more quantity fields. Suppress the expiry
        //       month field(s) not immediately preceding an expiry year field.
        if (!keep_cc_fields || !cc_date_found) {
          if (!cc_date_found) {
            LOG_AF(log_manager)
                << LoggingScope::kRationalization
                << LogMessage::kRationalization
                << "Credit card rationalization: Found CC expiration month but "
                   "not a full date.";
          }
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        } else if (num_months_found > 1) {
          auto it2 = it + 1;
          if (it2 == fields_->end()) {
            LOG_AF(log_manager)
                << LoggingScope::kRationalization
                << LogMessage::kRationalization
                << "Credit card rationalization: Found multiple expiration "
                   "months and the last field was an expiration month";
            field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          } else {
            FieldType next_field_type =
                (*it2)->ComputedType().GetStorableType();
            if (next_field_type != CREDIT_CARD_EXP_2_DIGIT_YEAR &&
                next_field_type != CREDIT_CARD_EXP_4_DIGIT_YEAR) {
              LOG_AF(log_manager)
                  << LoggingScope::kRationalization
                  << LogMessage::kRationalization
                  << "Credit card rationalization: Found multiple expiration "
                     "months and the field following one is not an "
                     "expiration year but "
                  << FieldTypeToStringView(next_field_type) << ".";
              field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
            }
          }
        }
        break;
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        if (!keep_cc_fields || !cc_date_found) {
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          if (!cc_date_found) {
            LOG_AF(log_manager)
                << LoggingScope::kRationalization
                << LogMessage::kRationalization
                << "Credit card rationalization: Found expiration year but no "
                   "full expiration date.";
          }
        }
        break;
      case CREDIT_CARD_VERIFICATION_CODE: {
        bool is_standalone_cvc_field = !cc_name_found && !cc_num_found &&
                                       !cc_date_found && !email_address_found;
        if (base::FeatureList::IsEnabled(
                features::kAutofillParseVcnCardOnFileStandaloneCvcFields) &&
            is_standalone_cvc_field) {
          // If there aren't any other credit card fields and no email address
          // field, than we presume this is a credit card saved on file of a
          // merchant webpage.
          field->SetTypeTo(
              AutofillType(CREDIT_CARD_STANDALONE_VERIFICATION_CODE));
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "Credit card rationalization: Found CVC field but no other "
                 "credit card fields or email address field. Changed to "
                 "standalone CVC field.";
        } else if (!keep_cc_fields) {
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        }
        break;
      }
      default:
        break;
    }
  }

  // If after rationalization we have an expiration date field, we consider
  // once more whether we should make this a field with a 2 or 4 digit
  // expiration year based on server information.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    for (const auto& field : *fields_) {
      // Here we look at the type after rationalization.
      FieldType current_field_type = field->Type().GetStorableType();
      if (current_field_type == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
          current_field_type == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
        FieldType server_hint = field->server_type();
        FieldType forced_field_type =
            field->server_type_prediction_is_override() ? server_hint
                                                        : NO_SERVER_DATA;
        CreditCardFieldParser::ExpirationDateFormat format =
            CreditCardFieldParser::DetermineExpirationDateFormat(
                *field, /*fallback_type=*/CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                /*server_hint=*/server_hint,
                /*forced_field_type=*/forced_field_type);
        FieldType new_field_type = format.digits_in_expiration_year == 4
                                       ? CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
                                       : CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
        if (new_field_type != current_field_type) {
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "Credit card rationalization: Updated expiration date format "
                 "with server hints or via patterns found in the labels.";
          field->SetTypeTo(AutofillType(new_field_type));
        }
      }
    }
  }
}

void FormStructureRationalizer::RationalizeMultiOriginCreditCardFields(
    const url::Origin& main_origin,
    LogManager* log_manager) {
  auto is_in_subframe = [&main_origin](const FormFieldData& field) {
    return field.origin() != main_origin;
  };
  auto rationalize = [&](FieldType relevant_type) {
    auto is_relevant = [relevant_type](const AutofillField& field) {
      return field.ComputedType().GetStorableType() == relevant_type;
    };
    auto is_relevant_in_subframe = [&](const auto& field) {
      return is_relevant(*field) && is_in_subframe(*field);
    };
    // If a relevant field exists in a sub-frame, we can ignore the
    // corresponding field in the main frame as it is probably a
    // misclassification.
    if (std::ranges::any_of(*fields_, is_relevant_in_subframe)) {
      for (auto& field : *fields_) {
        if (is_relevant(*field) && !is_in_subframe(*field)) {
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "Multi-origin Credit Card Rationalization: Converting type of "
              << field->global_id() << " from "
              << FieldTypeToStringView(relevant_type) << " to UNKNOWN_TYPE";
        }
      }
    }
  };
  // These fields do usually not occur on the main frame's origin due to
  // PCI-DSS. By contrast, cardholder name and expiration dates do commonly
  // appear in the main frame and in cross-origin iframes.
  rationalize(CREDIT_CARD_NUMBER);
  rationalize(CREDIT_CARD_VERIFICATION_CODE);
}

void FormStructureRationalizer::RationalizeCreditCardNumberOffsets(
    LogManager* log_manager) {
  // Credit card numbers are 8 to 19 digits in length.
  // [Ref: http://en.wikipedia.org/wiki/Bank_card_number]
  constexpr size_t kMinValidCardNumberSize = 8;
  constexpr size_t kMaxValidCardNumberSize = 19;
  constexpr size_t kMaxGroupElementLength = 8;
  using Group = base::span<const std::unique_ptr<AutofillField>>;

  // `may_be_group({f, f + 1}) && ... && may_be_group({f, f + N + 1})` is true
  // iff all fields in the range
  // 1. `{f, f + N + 1}` are credit card number fields, and
  // 2. `{f, f + N + 1}` originate from the same form in the same frame, and
  // 3. `{f, f + N + 1}` are all focusable or all unfocusable,
  // 4. `{f, f + N}` have the same `FormFieldData::max_length <
  //    kMaxGroupElementLength`.
  //
  // `may_be_group({f, f + N + 1})` is valid only if `may_be_group({f, f + N})`
  // is true. This is because each call only looks at the first and last
  // element.
  auto may_be_group = [](Group group) {
    DCHECK_GE(group.size(), 1u);
    DCHECK(
        std::ranges::all_of(group.first(group.size() - 1), [](const auto& f) {
          return f->ComputedType().GetStorableType() == CREDIT_CARD_NUMBER;
        }));
    size_t last = group.size() - 1;
    return group[0]->max_length() <= kMaxGroupElementLength &&
           group[last]->ComputedType().GetStorableType() ==
               CREDIT_CARD_NUMBER &&
           group[last]->renderer_form_id() == group[0]->renderer_form_id() &&
           group[last]->IsFocusable() == group[0]->IsFocusable() &&
           (last == 0 ||
            group[last - 1]->max_length() == group[0]->max_length());
  };

  // `has_reasonable_length({f, f + N + 1})` is true iff
  // 1. the cumulative FormFieldData::max_length
  //    (a) is long enough for the shortest credit cards, and
  //    (b) minus the overflow field (if present) isn't longer than the longest
  //        credit card, and
  // 2. there are at least 2 non-overflow fields.
  auto has_reasonable_length = [](Group group) {
    DCHECK(!group.empty());
    DCHECK(std::ranges::all_of(
        group.first(group.size() - 1), [group](const auto& f) {
          return f->max_length() == group[0]->max_length();
        }));
    size_t size = group.size();
    size_t last = group.size() - 1;
    bool last_is_overflow = group[last]->max_length() > kMaxGroupElementLength;
    size_t length =
        group[0]->max_length() * (size - 1) + group[last]->max_length();
    size_t length_without_overflow =
        length - last_is_overflow * group[last]->max_length();
    return length >= kMinValidCardNumberSize &&
           length_without_overflow <= kMaxValidCardNumberSize &&
           size >= 2 + last_is_overflow;
  };

  // Returns the end (exclusive) of the credit card number field group starting
  // with `begin`.
  auto find_end_of_group = [&](auto begin) {
    auto end = begin;
    while (end != fields_->end() && may_be_group({begin, end + 1})) {
      ++end;
    }
    return end;
  };

  for (const auto& field : *fields_) {
    field->set_credit_card_number_offset(0);
  }
  for (auto begin = fields_->begin(); begin != fields_->end();) {
    auto end = find_end_of_group(begin);
    if (begin == end) {
      begin = end + 1;
      continue;
    }
    // SAFETY: The iterators are from the same container.
    Group fields = Group(UNSAFE_BUFFERS({begin, end}));
    if (has_reasonable_length(fields)) {
      size_t offset = 0;
      for (auto& field : fields) {
        field->set_credit_card_number_offset(offset);
        offset += field->max_length();
      }
    }
    DCHECK(begin != end);
    begin = end;
  }
}

void FormStructureRationalizer::RationalizeStreetAddressAndAddressLine(
    LogManager* log_manager) {
  if (fields_->size() < 2)
    return;
  for (auto field = fields_->begin() + 1; field != fields_->end(); ++field) {
    if ((*field)->ComputedType().GetStorableType() != ADDRESS_HOME_LINE2)
      continue;
    // Rationalize a preceding street address belonging to the same section
    // unless it's a server override.
    AutofillField& previous_field = **(field - 1);
    if (previous_field.ComputedType().GetStorableType() !=
            ADDRESS_HOME_STREET_ADDRESS ||
        previous_field.section() != (*field)->section() ||
        previous_field.server_type_prediction_is_override()) {
      continue;
    }
    LOG_AF(log_manager)
        << LoggingScope::kRationalization << LogMessage::kRationalization
        << "Street Address Rationalization: Converting sequence of (street "
           "address, address line 2) to (address line 1, address line 2)";
    previous_field.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  }
}

void FormStructureRationalizer::RationalizeBetweenStreetFields(
    LogManager* log_manager) {
  if (fields_->size() < 2) {
    return;
  }
  for (auto field = fields_->begin(); field != fields_->end() - 1; ++field) {
    const bool first_is_between_streets =
        (*field)->ComputedType().GetStorableType() ==
        ADDRESS_HOME_BETWEEN_STREETS;
    if (!first_is_between_streets) {
      continue;
    }

    // Rationalize a preceding street address belonging to the same section
    // unless it's a server override.
    AutofillField& next_field = **(field + 1);
    const bool second_is_between_streets_1_or_2 =
        next_field.ComputedType().GetStorableType() ==
            ADDRESS_HOME_BETWEEN_STREETS_1 ||
        next_field.ComputedType().GetStorableType() ==
            ADDRESS_HOME_BETWEEN_STREETS_2;
    if (!second_is_between_streets_1_or_2) {
      continue;
    }
    LOG_AF(log_manager) << LoggingScope::kRationalization
                        << LogMessage::kRationalization
                        << "Address Home Between Streets Rationalization: "
                           "Converting sequence of (home_between_street,  "
                           "home_between_street_1) or (home_between_street, "
                           "home_between_street_2) to (home_between_street_1, "
                           "home_between_street_2)";
    (**field).SetTypeTo(AutofillType(ADDRESS_HOME_BETWEEN_STREETS_1));
    next_field.SetTypeTo(AutofillType(ADDRESS_HOME_BETWEEN_STREETS_2));
    break;
  }
}

void FormStructureRationalizer::RationalizePhoneNumberTrunkTypes(
    LogManager* log_manager) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForPhoneNumberTrunkTypes)) {
    return;
  }

  // Changes the `field`'s type to `new_type` if it isn't `new_type` already.
  // If the type is changed, logs to `log_manager`.
  auto change_type_and_log =
      [&](AutofillField& field, FieldType new_type) {
        FieldType current_type = field.ComputedType().GetStorableType();
        if (current_type == new_type) {
          return;
        }
        field.SetTypeTo(AutofillType(new_type));
        LOG_AF(log_manager)
            << LoggingScope::kRationalization << LogMessage::kRationalization
            << "Converting " << FieldTypeToStringView(current_type) << " to "
            << FieldTypeToStringView(new_type)
            << " as part of phone number trunk type rationalization";
      };

  // Indicates whether the previous field was a phone country code.
  bool preceding_phone_country_code = false;
  for (const std::unique_ptr<AutofillField>& field : *fields_) {
    FieldType type = field->ComputedType().GetStorableType();
    if (type == PHONE_HOME_CITY_AND_NUMBER ||
        type == PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX) {
      change_type_and_log(*field,
                          preceding_phone_country_code
                              ? PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX
                              : PHONE_HOME_CITY_AND_NUMBER);
    } else if (type == PHONE_HOME_CITY_CODE ||
               type == PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX) {
      change_type_and_log(*field, preceding_phone_country_code
                                      ? PHONE_HOME_CITY_CODE
                                      : PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX);
    }
    preceding_phone_country_code = type == PHONE_HOME_COUNTRY_CODE;
  }
}

void FormStructureRationalizer::RationalizePhoneNumbersForFilling() {
  std::map<Section, std::vector<AutofillField*>> section_fields;
  for (const std::unique_ptr<AutofillField>& field : *fields_) {
    section_fields[field->section()].push_back(field.get());
  }
  for (auto& [section, fields] : section_fields) {
    autofill::RationalizePhoneNumbersForFilling(fields);
  }
}

void FormStructureRationalizer::RationalizeRepeatedStreetAddressFields(
    LogManager* log_manager) {
  // Group ADDRESS_HOME_STREET_ADDRESS `fields_` by section.
  std::map<Section, std::vector<AutofillField*>> street_address_fields;
  for (const std::unique_ptr<AutofillField>& field : *fields_) {
    if (field->IsFocusable() && field->ComputedType().GetStorableType() ==
                                    ADDRESS_HOME_STREET_ADDRESS) {
      street_address_fields[field->section()].push_back(field.get());
    }
  }

  constexpr static std::array<FieldType, 3> kAddressLineTypes = {
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2, ADDRESS_HOME_LINE3};
  // Rationalise the street address fields in every section.
  for (auto& [section, fields] : street_address_fields) {
    if (fields.size() != 2 && fields.size() != 3) {
      continue;
    }
    auto next_type = kAddressLineTypes.begin();
    for (AutofillField* field : fields) {
      LOG_AF(log_manager)
          << LoggingScope::kRationalization << LogMessage::kRationalization
          << "RationalizeAddressLineFields ADDRESS_HOME_STREET_ADDRESS to "
          << FieldTypeToString(*next_type);
      field->SetTypeTo(AutofillType(*next_type));
      ++next_type;
    }
  }
}

void FormStructureRationalizer::RationalizeFieldTypePredictions(
    const url::Origin& main_origin,
    const GeoIpCountryCode& client_country,
    const LanguageCode& language_code,
    LogManager* log_manager) {
  RationalizeCreditCardFieldPredictions(log_manager);
  RationalizeMultiOriginCreditCardFields(main_origin, log_manager);
  RationalizeCreditCardNumberOffsets(log_manager);
  RationalizeRepeatedStreetAddressFields(log_manager);
  RationalizeStreetAddressAndAddressLine(log_manager);
  RationalizeBetweenStreetFields(log_manager);
  RationalizePhoneNumberTrunkTypes(log_manager);
  RationalizePhoneCountryCode(log_manager);
  RationalizeByRationalizationEngine(client_country, language_code,
                                     log_manager);
}

void FormStructureRationalizer::RationalizePhoneCountryCode(
    LogManager* log_manager) {
  constexpr static FieldTypeSet kRelevantPhoneTypes{
      PHONE_HOME_NUMBER, PHONE_HOME_NUMBER_PREFIX, PHONE_HOME_CITY_AND_NUMBER,
      PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX};
  if (std::ranges::any_of(*fields_, [&](const auto& field) {
        return kRelevantPhoneTypes.contains(
            field->ComputedType().GetStorableType());
      })) {
    return;
  }
  for (const std::unique_ptr<AutofillField>& field : *fields_) {
    if (field->ComputedType().GetStorableType() == PHONE_HOME_COUNTRY_CODE) {
      field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
      LOG_AF(log_manager)
          << "RationalizeTypeRelationships: Fields of type "
             "PHONE_HOME_COUNTRY_CODE can only coexist with other"
             "phone number types.";
    }
  }
}

void FormStructureRationalizer::RationalizeByRationalizationEngine(
    const GeoIpCountryCode& client_country,
    const LanguageCode& language_code,
    LogManager* log_manager) {
  ParsingContext context(client_country, language_code,
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
                         PatternFile::kDefault,
#else
                         PatternFile::kLegacy,
#endif
                         GetActiveRegexFeatures());

  rationalization::ApplyRationalizationEngineRules(context, *fields_,
                                                   log_manager);
}

}  // namespace autofill
