// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/address_on_typing/address_on_typing_manager.h"

#include <optional>

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/strike_databases/addresses/address_on_typing_suggestion_strike_database.h"
#include "components/autofill/core/common/dense_set.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

AddressOnTypingManager::AddressOnTypingManager(AutofillClient& client)
    : client_(client) {}

AddressOnTypingManager::~AddressOnTypingManager() {
  // If suggestions were shown but not accepted for a field, add a strike for
  // all the field types where a suggestion was shown.
  for (FieldType field_type_ignored : unaccepted_field_types_) {
    AddStrikeToBlockAddressOnTypingSuggestions(field_type_ignored);
    if (GetAddressOnTypingMaxStrikesLimit() &&
        GetAddressOnTypingMaxStrikesLimit() ==
            GetAddressOnTypingFieldTypeStrikes(field_type_ignored)) {
      base::UmaHistogramSparse(
          "Autofill.AddressSuggestionOnTypingFieldTypeAddedToStrikeDatabase",
          field_type_ignored);
    }
  }
  // Once a `SuggestionType::kAutofillAddressOnTyping` suggestion
  // is accepted, we remove it from
  // `fields_where_address_on_typing_was_shown_`. Therefore for
  // the remaining fields, log that they were not accepted
  for (const auto& [field_global_id,
                    triggering_field_classification_and_field_types_used] :
       fields_where_address_on_typing_was_shown_) {
    base::UmaHistogramBoolean(
        "Autofill.AddressSuggestionOnTypingAcceptance.Any", false);
    const bool triggering_field_classified =
        triggering_field_classification_and_field_types_used.first;
    if (triggering_field_classified) {
      base::UmaHistogramBoolean(
          "Autofill.AddressSuggestionOnTypingAcceptance.Classified", false);
    } else {
      base::UmaHistogramBoolean(
          "Autofill.AddressSuggestionOnTypingAcceptance.Unclassified", false);
    }
    FieldTypeSet field_types_used_in_suggestions_generation =
        triggering_field_classification_and_field_types_used.second;
    for (FieldType field_type : field_types_used_in_suggestions_generation) {
      base::UmaHistogramSparse(
          "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType",
          autofill_metrics::GetBucketForAcceptanceMetricsGroupedByFieldType(
              field_type, /*suggestion_accepted=*/false));
    }
  }

  // Log information about `SuggestionType::kAutofillAddressOnTyping`
  // suggestions and profile usage.
  for (auto [guid, last_used_time] :
       address_on_typing_suggestion_profile_last_used_time_per_guid_) {
    base::UmaHistogramCounts1000(
        "Autofill.AddressSuggestionOnTypingShown.DaysSinceLastUse.Profile",
        last_used_time.InDays());
  }

  for (const std::string& profile_accepted_guid :
       address_on_typing_suggestion_accepted_profile_used_) {
    base::UmaHistogramCounts1000(
        "Autofill.AddressSuggestionOnTypingAccepted.DaysSinceLastUse.Profile",
        address_on_typing_suggestion_profile_last_used_time_per_guid_
            [profile_accepted_guid]
                .InDays());
  }
}

void AddressOnTypingManager::OnDidShowAddressOnTyping(
    FieldGlobalId field_global_id,
    AutofillField* autofill_field) {
  FieldTypeSet field_types_used;
  absl::flat_hash_map<std::string, base::TimeDelta>
      profile_last_used_time_per_guid;
  const base::Time now = base::Time::Now();
  for (const Suggestion& suggestion : client_->GetAutofillSuggestions()) {
    if (suggestion.type != SuggestionType::kAddressEntryOnTyping) {
      continue;
    }

    const Suggestion::AutofillProfilePayload& profile_used_payload =
        std::get<Suggestion::AutofillProfilePayload>(suggestion.payload);
    const AutofillProfile& profile_used =
        CHECK_DEREF(address_data_manager().GetProfileByGUID(
            profile_used_payload.guid.value()));

    profile_last_used_time_per_guid[profile_used_payload.guid.value()] =
        now - profile_used.usage_history().use_date();
    field_types_used.insert(*suggestion.field_by_field_filling_type_used);
  }

  FieldTypeSet triggering_field_types =
      autofill_field ? autofill_field->Type().GetTypes() : FieldTypeSet{};
  unaccepted_field_types_.insert_all(field_types_used);
  if (fields_where_address_on_typing_was_shown_.contains(field_global_id)) {
    fields_where_address_on_typing_was_shown_[field_global_id]
        .second.insert_all(field_types_used);
  } else {
    const bool is_triggering_field_classified =
        !FieldTypeSet{NO_SERVER_DATA, UNKNOWN_TYPE, EMPTY_TYPE}.contains_all(
            triggering_field_types);
    fields_where_address_on_typing_was_shown_[field_global_id] = {
        is_triggering_field_classified, field_types_used};
  }

  for (auto [guid, last_used_time] : profile_last_used_time_per_guid) {
    address_on_typing_suggestion_profile_last_used_time_per_guid_[guid] =
        last_used_time;
  }
}

void AddressOnTypingManager::OnDidAcceptAddressOnTyping(
    FieldGlobalId field_global_id,
    const std::u16string& value,
    FieldType field_type_used_to_build_suggestion,
    const std::string profile_used_guid) {
  CHECK(unaccepted_field_types_.contains(field_type_used_to_build_suggestion));
  unaccepted_field_types_.erase(field_type_used_to_build_suggestion);
  // The user accepted a suggestion, clear all strikes for this
  // field type.
  RemoveStrikesToBlockAddressOnTypingSuggestions(
      field_type_used_to_build_suggestion);

  CHECK(fields_where_address_on_typing_was_shown_.contains(field_global_id));
  CHECK(address_on_typing_suggestion_profile_last_used_time_per_guid_.contains(
      profile_used_guid));
  base::UmaHistogramBoolean("Autofill.AddressSuggestionOnTypingAcceptance.Any",
                            true);
  if (fields_where_address_on_typing_was_shown_[field_global_id].first) {
    base::UmaHistogramBoolean(
        "Autofill.AddressSuggestionOnTypingAcceptance.Classified", true);
  } else {
    base::UmaHistogramBoolean(
        "Autofill.AddressSuggestionOnTypingAcceptance.Unclassified", true);
  }

  // Sanity check to avoid later division by zero.
  if (value.empty()) {
    return;
  }

  address_on_typing_value_used_[field_global_id] = value;
  for (FieldType field_type :
       fields_where_address_on_typing_was_shown_[field_global_id].second) {
    base::UmaHistogramSparse(
        "Autofill.AddressSuggestionOnTypingAcceptance.PerFieldType",
        autofill_metrics::GetBucketForAcceptanceMetricsGroupedByFieldType(
            field_type, /*suggestion_accepted=*/field_type ==
                            field_type_used_to_build_suggestion));
  }
  // Stores the accepted profile and log on destruction as a way to avoid
  // logging acceptance multiple times for the same profile.
  address_on_typing_suggestion_accepted_profile_used_.insert(profile_used_guid);
  fields_where_address_on_typing_was_shown_.erase(field_global_id);
}

void AddressOnTypingManager::LogAddressOnTypingCorrectnessMetrics(
    const FormStructure& form) {
  const std::vector<std::unique_ptr<AutofillField>>& submitted_form_fields =
      form.fields();

  // For each field in the submitted form, record its value.
  auto submitted_fields_values =
      base::MakeFlatMap<FieldGlobalId, std::u16string>(
          submitted_form_fields, {},
          [](const std::unique_ptr<AutofillField>& field) {
            return std::make_pair(field->global_id(), field->value());
          });
  // Used to delete fields for which correctness was logged from
  // `address_on_typing_value_used_`.
  std::set<FieldGlobalId> logged_correctness_for_field;
  for (const auto& [field_global_id, filled_value] :
       address_on_typing_value_used_) {
    if (submitted_fields_values.contains(field_global_id)) {
      const std::u16string submitted_value =
          submitted_fields_values.at(field_global_id);
      base::UmaHistogramBoolean(
          "Autofill.EditedAutofilledFieldAtSubmission.AddressOnTyping",
          filled_value == submitted_fields_values.at(field_global_id));
      logged_correctness_for_field.insert(field_global_id);
      size_t filled_value_and_submitted_value_distance =
          base::LevenshteinDistance(filled_value, submitted_value);
      base::UmaHistogramCounts100(
          "Autofill.EditedDistanceAutofilledFieldAtSubmission.AddressOnTyping",
          filled_value_and_submitted_value_distance);

      int edited_percentage = 100 * filled_value_and_submitted_value_distance /
                              filled_value.length();
      base::UmaHistogramCounts100(
          "Autofill.EditedPercentageAutofilledFieldAtSubmission."
          "AddressOnTyping",
          edited_percentage);
    }
  }

  // Remove from `address_on_typing_value_used_` fields for which correctness
  // metrics were logged.
  for (const FieldGlobalId field : logged_correctness_for_field) {
    address_on_typing_value_used_.erase(field);
  }
}

void AddressOnTypingManager::AddStrikeToBlockAddressOnTypingSuggestions(
    FieldType field_type) {
  if (AddressOnTypingSuggestionStrikeDatabase* strike_database =
          address_on_typing_strike_database()) {
    strike_database->AddStrike(base::NumberToString(field_type));
  }
}

std::optional<int> AddressOnTypingManager::GetAddressOnTypingMaxStrikesLimit()
    const {
  if (const AddressOnTypingSuggestionStrikeDatabase* strike_database =
          address_on_typing_strike_database()) {
    return strike_database->GetMaxStrikesLimit();
  }
  return std::nullopt;
}

std::optional<int> AddressOnTypingManager::GetAddressOnTypingFieldTypeStrikes(
    FieldType field_type) const {
  if (const AddressOnTypingSuggestionStrikeDatabase* strike_database =
          address_on_typing_strike_database()) {
    return strike_database->GetStrikes(base::NumberToString(field_type));
  }
  return std::nullopt;
}

void AddressOnTypingManager::RemoveStrikesToBlockAddressOnTypingSuggestions(
    FieldType field_type) {
  if (AddressOnTypingSuggestionStrikeDatabase* strike_database =
          address_on_typing_strike_database()) {
    return strike_database->ClearStrikes(base::NumberToString(field_type));
  }
}

AddressDataManager& AddressOnTypingManager::address_data_manager() const {
  return client_->GetPersonalDataManager().address_data_manager();
}

AddressOnTypingSuggestionStrikeDatabase*
AddressOnTypingManager::address_on_typing_strike_database() {
  return address_data_manager().GetAddressOnTypingSuggestionStrikeDatabase();
}

const AddressOnTypingSuggestionStrikeDatabase*
AddressOnTypingManager::address_on_typing_strike_database() const {
  return address_data_manager().GetAddressOnTypingSuggestionStrikeDatabase();
}

}  // namespace autofill
