// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure_rationalizer.h"

#include "base/containers/contains.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/rationalization_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

namespace {

// Defines necessary types for the rationalization logic, meaning that fields of
// `type` are only filled if at least one field of some `GetNecessaryTypesFor()`
// is present.
// TODO(crbug.com/1311937) Cleanup PHONE_HOME_CITY_AND_NUMBER when launched.
ServerFieldTypeSet GetNecessaryTypesFor(ServerFieldType type) {
  switch (type) {
    case PHONE_HOME_COUNTRY_CODE:
      return {PHONE_HOME_NUMBER, PHONE_HOME_NUMBER_PREFIX,
              base::FeatureList::IsEnabled(
                  features::kAutofillEnableSupportForPhoneNumberTrunkTypes)
                  ? PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX
                  : PHONE_HOME_CITY_AND_NUMBER};
    default:
      return {};
  }
}

}  // namespace

class FormStructureRationalizer::SectionedFieldsIndexes {
 public:
  SectionedFieldsIndexes() = default;
  ~SectionedFieldsIndexes() = default;

  size_t LastFieldIndex() const {
    if (sectioned_indexes_.empty())
      return std::numeric_limits<size_t>::max();  // Shouldn't happen.
    return sectioned_indexes_.back().back();
  }

  void AddFieldIndex(const size_t index, bool is_new_section) {
    if (is_new_section || Empty()) {
      sectioned_indexes_.emplace_back();
    }
    sectioned_indexes_.back().push_back(index);
  }

  void WalkForwardToTheNextSection() { current_section_ptr_++; }

  bool IsFinished() const {
    return current_section_ptr_ >= sectioned_indexes_.size();
  }

  size_t CurrentIndex() const {
    return current_section_ptr_ < sectioned_indexes_.size()
               ? sectioned_indexes_[current_section_ptr_].front()
               : std::numeric_limits<size_t>::max();
  }

  const std::vector<size_t>* CurrentSection() const {
    return current_section_ptr_ < sectioned_indexes_.size()
               ? &sectioned_indexes_[current_section_ptr_]
               : nullptr;
  }

  void Reset() { current_section_ptr_ = 0; }

  bool Empty() const { return sectioned_indexes_.empty(); }

 private:
  // A vector of sections. Each section is a vector of some of the indexes
  // that belong to the same section. The sections and indexes are sorted by
  // their order of appearance on the form.
  std::vector<std::vector<size_t>> sectioned_indexes_;
  // Points to a vector of indexes that belong to the same section.
  size_t current_section_ptr_ = 0;
};

FormStructureRationalizer::FormStructureRationalizer(
    std::vector<std::unique_ptr<AutofillField>>* fields,
    FormSignature form_signature)
    : fields_(*fields), form_signature_(form_signature) {}
FormStructureRationalizer::~FormStructureRationalizer() = default;

void FormStructureRationalizer::RationalizeCreditCardFieldPredictions(
    LogManager* log_manager) {
  bool cc_first_name_found = false;
  bool cc_last_name_found = false;
  bool cc_num_found = false;
  bool cc_month_found = false;
  bool cc_year_found = false;
  bool cc_type_found = false;
  bool cc_cvc_found = false;
  size_t num_months_found = 0;
  size_t num_other_fields_found = 0;
  for (const auto& field : *fields_) {
    ServerFieldType current_field_type =
        field->ComputedType().GetStorableType();
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
    ServerFieldType current_field_type = field->Type().GetStorableType();
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
      case CREDIT_CARD_VERIFICATION_CODE:
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
            ServerFieldType next_field_type = (*it2)->Type().GetStorableType();
            if (next_field_type != CREDIT_CARD_EXP_2_DIGIT_YEAR &&
                next_field_type != CREDIT_CARD_EXP_4_DIGIT_YEAR) {
              LOG_AF(log_manager)
                  << LoggingScope::kRationalization
                  << LogMessage::kRationalization
                  << "Credit card rationalization: Found multiple expiration "
                     "months and the field following one is not an "
                     "expiration year but "
                  << FieldTypeToStringPiece(next_field_type) << ".";
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
                   "full expriration date.";
          }
        }
        break;
      default:
        break;
    }
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
        previous_field.section != (*field)->section ||
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

void FormStructureRationalizer::RationalizeStreetAddressAndHouseNumber(
    LogManager* log_manager) {
  if (fields_->size() < 2)
    return;
  for (auto field = fields_->begin() + 1; field != fields_->end(); ++field) {
    if ((*field)->ComputedType().GetStorableType() != ADDRESS_HOME_HOUSE_NUMBER)
      continue;
    // Rationalize a preceding street address belonging to the same section
    // unless it's a server override.
    AutofillField& previous_field = **(field - 1);

    ServerFieldType previous_type =
        previous_field.ComputedType().GetStorableType();
    // We intentionally consider a street address and address-line1, but not
    // address-line2. There are cases where an address-line2 has a separate
    // meaning (overflow field) and the logic implicitly assumes an order of
    // street name -> house number. It does not support house number ->
    // street name.
    bool is_street_address_type =
        previous_type == ADDRESS_HOME_STREET_ADDRESS ||
        previous_type == ADDRESS_HOME_LINE1;
    if (!is_street_address_type ||
        previous_field.section != (*field)->section ||
        previous_field.server_type_prediction_is_override()) {
      continue;
    }
    // TODO(crbug.com/1326425): Remove once feature is lanuched.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillRationalizeStreetAddressAndHouseNumber)) {
      continue;
    }
    LOG_AF(log_manager)
        << LoggingScope::kRationalization << LogMessage::kRationalization
        << "Street Address Rationalization: Converting sequence of (street "
           "address/address-line1, house number) to (street name, house "
           "number)";
    previous_field.SetTypeTo(AutofillType(ADDRESS_HOME_STREET_NAME));
  }
}

void FormStructureRationalizer::RationalizePhoneNumbersInSection(
    const Section& section) {
  std::vector<AutofillField*> fields;
  for (const auto& field : *fields_) {
    if (field->section != section)
      continue;
    fields.push_back(field.get());
  }
  rationalization_util::RationalizePhoneNumberFields(fields);
}

void FormStructureRationalizer::ApplyRationalizationsToFieldAndLog(
    size_t field_index,
    ServerFieldType new_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  if (field_index >= fields_->size())
    return;
  auto old_type = (*fields_)[field_index]->Type().GetStorableType();
  (*fields_)[field_index]->SetTypeTo(AutofillType(new_type));
  if (form_interactions_ukm_logger) {
    form_interactions_ukm_logger->LogRepeatedServerTypePredictionRationalized(
        form_signature_, *(*fields_)[field_index], old_type);
  }
}

void FormStructureRationalizer::RationalizeAddressLineFields(
    SectionedFieldsIndexes* sections_of_address_indexes,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  // The rationalization happens within sections.
  for (sections_of_address_indexes->Reset();
       !sections_of_address_indexes->IsFinished();
       sections_of_address_indexes->WalkForwardToTheNextSection()) {
    auto* current_section = sections_of_address_indexes->CurrentSection();

    // The rationalization only applies to sections that have 2 or 3 visible
    // street address predictions.
    if (!current_section ||
        (current_section->size() != 2 && current_section->size() != 3)) {
      continue;
    }

    int nb_address_rationalized = 0;
    for (auto field_index : *current_section) {
      LOG_AF(log_manager)
          << LoggingScope::kRationalization << LogMessage::kRationalization
          << "RationalizeAddressLineFields ADDRESS_HOME_STREET_ADDRESS to ";
      switch (nb_address_rationalized) {
        case 0:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE1,
                                             form_interactions_ukm_logger);
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "ADDRESS_HOME_LINE1";
          break;
        case 1:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE2,
                                             form_interactions_ukm_logger);
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "ADDRESS_HOME_LINE2";
          break;
        case 2:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE3,
                                             form_interactions_ukm_logger);
          LOG_AF(log_manager)
              << LoggingScope::kRationalization << LogMessage::kRationalization
              << "ADDRESS_HOME_LINE3";
          break;
        default:
          NOTREACHED();
          break;
      }
      ++nb_address_rationalized;
    }
  }
}

void FormStructureRationalizer::ApplyRationalizationsToHiddenSelects(
    size_t field_index,
    ServerFieldType new_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  ServerFieldType old_type = (*fields_)[field_index]->Type().GetStorableType();

  // Walk on the unfocusable select fields right after the field_index which
  // share the same type with the field_index, and apply the rationalization to
  // them as well. These fields, if any, function as one field with the
  // field_index.
  for (auto current_index = field_index + 1; current_index < fields_->size();
       current_index++) {
    if ((*fields_)[current_index]->IsFocusable() ||
        (*fields_)[current_index]->form_control_type != "select-one" ||
        (*fields_)[current_index]->Type().GetStorableType() != old_type)
      break;
    ApplyRationalizationsToFieldAndLog(current_index, new_type,
                                       form_interactions_ukm_logger);
  }

  // Same for the fields coming right before the field_index. (No need to check
  // for the fields appearing before the first field!)
  if (field_index == 0)
    return;
  for (auto current_index = field_index - 1;; current_index--) {
    if ((*fields_)[current_index]->IsFocusable() ||
        (*fields_)[current_index]->form_control_type != "select-one" ||
        (*fields_)[current_index]->Type().GetStorableType() != old_type)
      break;
    ApplyRationalizationsToFieldAndLog(current_index, new_type,
                                       form_interactions_ukm_logger);
    if (current_index == 0)
      break;
  }
}

bool FormStructureRationalizer::HeuristicsPredictionsAreApplicable(
    size_t upper_index,
    size_t lower_index,
    ServerFieldType first_type,
    ServerFieldType second_type) {
  // The predictions are applicable if one field has one of the two types, and
  // the other has the other type.
  if ((*fields_)[upper_index]->heuristic_type() ==
      (*fields_)[lower_index]->heuristic_type())
    return false;
  if (((*fields_)[upper_index]->heuristic_type() == first_type ||
       (*fields_)[upper_index]->heuristic_type() == second_type) &&
      ((*fields_)[lower_index]->heuristic_type() == first_type ||
       (*fields_)[lower_index]->heuristic_type() == second_type))
    return true;
  return false;
}

void FormStructureRationalizer::ApplyRationalizationsToFields(
    size_t upper_index,
    size_t lower_index,
    ServerFieldType upper_type,
    ServerFieldType lower_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  // Unfocusable fields are ignored during the rationalization, but unfocusable
  // 'select' fields also get autofilled to support their corresponding visible
  // 'synthetic fields'. So, if a field's type is rationalized, we should make
  // sure that the rationalization is also applied to its corresponding
  // unfocusable fields, if any.
  ApplyRationalizationsToHiddenSelects(upper_index, upper_type,
                                       form_interactions_ukm_logger);
  ApplyRationalizationsToFieldAndLog(upper_index, upper_type,
                                     form_interactions_ukm_logger);

  ApplyRationalizationsToHiddenSelects(lower_index, lower_type,
                                       form_interactions_ukm_logger);
  ApplyRationalizationsToFieldAndLog(lower_index, lower_type,
                                     form_interactions_ukm_logger);
}

bool FormStructureRationalizer::FieldShouldBeRationalizedToCountry(
    size_t upper_index) {
  // Upper field is country if and only if it's the first visible address field
  // in its section. Otherwise, the upper field is a state, and the lower one
  // is a country.
  for (int field_index = upper_index - 1; field_index >= 0; --field_index) {
    if ((*fields_)[field_index]->IsFocusable() &&
        AutofillType((*fields_)[field_index]->Type().GetStorableType())
                .group() == FieldTypeGroup::kAddressHome &&
        (*fields_)[field_index]->section == (*fields_)[upper_index]->section) {
      return false;
    }
  }
  return true;
}

void FormStructureRationalizer::RationalizeAddressStateCountry(
    SectionedFieldsIndexes* sections_of_state_indexes,
    SectionedFieldsIndexes* sections_of_country_indexes,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  // Walk on the sections of state and country indexes simultaneously. If they
  // both point to the same section, it means that the section includes both the
  // country and the state type. This means that no rationalization is needed.
  // So, walk both pointers forward. Otherwise, look at the section that appears
  // earlier on the form. That section doesn't have any field of the other type.
  // Rationalize the fields on the earlier section if needed. Walk the pointer
  // that points to the earlier section forward. Stop when both sections of
  // indexes are processed. (This resembles the merge in the merge sort.)
  sections_of_state_indexes->Reset();
  sections_of_country_indexes->Reset();

  while (!sections_of_state_indexes->IsFinished() ||
         !sections_of_country_indexes->IsFinished()) {
    // If there are still sections left with both country and state type, and
    // state and country current sections are equal, then that section has both
    // state and country. No rationalization needed.
    if (!sections_of_state_indexes->IsFinished() &&
        !sections_of_country_indexes->IsFinished() &&
        (*fields_)[sections_of_state_indexes->CurrentIndex()]->section ==
            (*fields_)[sections_of_country_indexes->CurrentIndex()]->section) {
      sections_of_state_indexes->WalkForwardToTheNextSection();
      sections_of_country_indexes->WalkForwardToTheNextSection();
      continue;
    }

    size_t upper_index = 0;
    size_t lower_index = 0;

    auto* current_section_of_state_indexes =
        sections_of_state_indexes->CurrentSection();
    auto* current_section_of_country_indexes =
        sections_of_country_indexes->CurrentSection();
    DCHECK(current_section_of_state_indexes ||
           current_section_of_country_indexes);

    // If country section is before the state ones, it means that that section
    // misses states, and the other way around.
    if (!current_section_of_country_indexes ||
        (current_section_of_state_indexes &&
         *current_section_of_state_indexes <
             *current_section_of_country_indexes)) {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_state_indexes->size() == 2) {
        upper_index = (*current_section_of_state_indexes)[0];
        lower_index = (*current_section_of_state_indexes)[1];
      }
      sections_of_state_indexes->WalkForwardToTheNextSection();
    } else {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_country_indexes->size() == 2) {
        upper_index = (*current_section_of_country_indexes)[0];
        lower_index = (*current_section_of_country_indexes)[1];
      }
      sections_of_country_indexes->WalkForwardToTheNextSection();
    }

    // This is when upper and lower indexes are not changed, meaning that there
    // is no need for rationalization.
    if (upper_index == lower_index) {
      continue;
    }

    if (HeuristicsPredictionsAreApplicable(upper_index, lower_index,
                                           ADDRESS_HOME_STATE,
                                           ADDRESS_HOME_COUNTRY)) {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    (*fields_)[upper_index]->heuristic_type(),
                                    (*fields_)[lower_index]->heuristic_type(),
                                    form_interactions_ukm_logger);
      LOG_AF(log_manager)
          << LoggingScope::kRationalization << LogMessage::kRationalization
          << "RationalizeAddressStateCountry: Heuristics are applicable";
      continue;
    }

    if (FieldShouldBeRationalizedToCountry(upper_index)) {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_COUNTRY, ADDRESS_HOME_STATE,
                                    form_interactions_ukm_logger);
      LOG_AF(log_manager) << LoggingScope::kRationalization
                          << LogMessage::kRationalization
                          << "RationalizeAddressStateCountry: "
                             "FieldShouldBeRationalizedToCountry";
    } else {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY,
                                    form_interactions_ukm_logger);
      LOG_AF(log_manager) << LoggingScope::kRationalization
                          << LogMessage::kRationalization
                          << "RationalizeAddressStateCountry: "
                             "!FieldShouldBeRationalizedToCountry";
    }
  }
}

void FormStructureRationalizer::RationalizeRepeatedFields(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  // The type of every field whose index is in
  // sectioned_field_indexes_by_type[|type|] is predicted by server as |type|.
  // Example: sectioned_field_indexes_by_type[FULL_NAME] is a sectioned fields
  // indexes of fields whose types are predicted as FULL_NAME by the server.
  SectionedFieldsIndexes sectioned_field_indexes_by_type[MAX_VALID_FIELD_TYPE];

  for (size_t i = 0; i < fields_->size(); ++i) {
    const AutofillField& field = *(*fields_)[i];
    // The unfocusable fields are considered invisible and therefore not
    // considered when rationalizing.
    if (!field.IsFocusable())
      continue;
    // The billing and non-billing types are aggregated.
    auto current_type = field.Type().GetStorableType();

    if (current_type != UNKNOWN_TYPE && current_type < MAX_VALID_FIELD_TYPE) {
      // Look at the sectioned field indexes for the current type, if the
      // current field belongs to that section, then the field index should be
      // added to that same section, otherwise, start a new section.
      sectioned_field_indexes_by_type[current_type].AddFieldIndex(
          i,
          /*is_new_section*/ sectioned_field_indexes_by_type[current_type]
                  .Empty() ||
              (*fields_)[sectioned_field_indexes_by_type[current_type]
                             .LastFieldIndex()]
                      ->section != field.section);
    }
  }

  RationalizeAddressLineFields(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STREET_ADDRESS]),
      form_interactions_ukm_logger, log_manager);
  RationalizeAddressStateCountry(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STATE]),
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_COUNTRY]),
      form_interactions_ukm_logger, log_manager);
}

void FormStructureRationalizer::RationalizeFieldTypePredictions(
    LogManager* log_manager) {
  RationalizeCreditCardFieldPredictions(log_manager);
  RationalizeStreetAddressAndAddressLine(log_manager);
  RationalizeStreetAddressAndHouseNumber(log_manager);
  for (const auto& field : *fields_)
    field->SetTypeTo(field->Type());
  RationalizeTypeRelationships(log_manager);
}

void FormStructureRationalizer::RationalizeTypeRelationships(
    LogManager* log_manager) {
  // Create a local set of all the types for faster lookup.
  ServerFieldTypeSet types;
  for (const auto& field : *fields_) {
    types.insert(field->Type().GetStorableType());
  }

  for (const auto& field : *fields_) {
    ServerFieldType field_type = field->Type().GetStorableType();
    ServerFieldTypeSet necessary_types = GetNecessaryTypesFor(field_type);
    if (!necessary_types.empty() && !types.contains_any(necessary_types)) {
      // We have relationship rules for this type, but no `neccessary_type` was
      // found. Disabling Autofill for this field.
      field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
      LOG_AF(log_manager)
          << "RationalizeTypeRelationships: Fields of type "
          << FieldTypeToStringPiece(field_type)
          << " can only exist if other fields of specific types exist.";
    }
  }
}

}  // namespace autofill
