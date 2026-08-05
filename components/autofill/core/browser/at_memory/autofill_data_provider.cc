// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/extend.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::personal_context::proto::TypedValue;

constexpr size_t kVisibleSuffixLength = 4;

// Extracts a `TypedValue` from `attribute` if there is a non-empty one.
std::optional<TypedValue> GetAttributeTypedValue(
    const AttributeInstance& attribute) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTypedFetchPlan)) {
    return std::nullopt;
  }
  TypedValue typed_val = attribute.GetTypedValue();
  return typed_val.value_case() == TypedValue::VALUE_NOT_SET
             ? std::nullopt
             : std::optional(std::move(typed_val));
}

// Extracts the expiration date as a `TypedValue` if it is non-empty.
std::optional<TypedValue> GetExpirationDateTypedValue(const CreditCard& card) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTypedFetchPlan)) {
    return std::nullopt;
  }
  const int expiration_year = card.expiration_year();
  const int expiration_month = card.expiration_month();
  if (expiration_year == 0 || expiration_month == 0) {
    return std::nullopt;
  }

  TypedValue typed_val;
  typed_val.mutable_date()->set_year(expiration_year);
  typed_val.mutable_date()->set_month(expiration_month);

  return typed_val;
}

// Extracts the home country as a `TypedValue` if it is non-empty.
std::optional<TypedValue> GetHomeCountryTypedValue(
    const AutofillProfile& profile) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTypedFetchPlan)) {
    return std::nullopt;
  }
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (country_code.length() != 2) {
    return std::nullopt;
  }
  TypedValue typed_val;
  typed_val.set_country_code(country_code);
  return typed_val;
}

// Adds metadata from `form_group` to `entry` if `metadata_entry_type` maps to
// a `FieldType` and differs from `primary_field_type`.
void AddMetadataToResult(MemorySearchResult& entry,
                         const FormGroup& form_group,
                         MemoryDataType metadata_entry_type,
                         FieldType primary_field_type,
                         const std::string& app_locale) {
  std::optional<FieldType> other_field_type = ToFieldType(metadata_entry_type);
  if (!other_field_type || other_field_type == primary_field_type) {
    return;
  }
  std::u16string metadata_value =
      form_group.GetInfo(*other_field_type, app_locale);
  if (!metadata_value.empty()) {
    entry.metadata_list.emplace_back(
        metadata_entry_type, GetMemoryDataTypeNameForI18n(metadata_entry_type),
        std::move(metadata_value));
  }
}

// Calculates a ranking score for an entity, based on frequency and recency of
// use.
double CalculateRankingScore(int64_t use_count, base::Time use_date) {
  UsageHistoryInformation usage_history;
  usage_history.set_use_count(use_count);
  usage_history.set_use_date(use_date);
  return usage_history.GetRankingScore(base::Time::Now());
}

// Builds a list of metadata entries from all non-empty attributes of an
// entity, optionally excluding a specific `excluded_type`.
std::vector<EntryMetadata> GetMetadataFromEntityAttributes(
    const EntityInstance& entity,
    std::string_view app_locale,
    AttributeType excluded_type) {
  std::vector<EntryMetadata> metadata;
  metadata.reserve(entity.attributes().size());
  for (const AttributeInstance& attr : entity.attributes()) {
    if (attr.type() == excluded_type) {
      continue;
    }
    std::u16string attr_value = attr.GetCompleteInfo(app_locale);
    if (attr_value.empty()) {
      continue;
    }
    if (attr.type().is_obfuscated()) {
      attr_value = GetObfuscatedValue(attr_value, kVisibleSuffixLength);
    }
    MemoryDataType metadata_type = AttributeTypeToMemoryDataType(attr.type());
    metadata.emplace_back(metadata_type,
                          GetMemoryDataTypeNameForI18n(metadata_type),
                          std::move(attr_value), GetAttributeTypedValue(attr));
  }
  return metadata;
}

MemorySearchResult CreateMemorySearchResultForEntity(
    const EntityInstance& entity,
    AttributeType primary_attribute_type,
    std::u16string value,
    std::string_view app_locale) {
  if (primary_attribute_type.is_obfuscated()) {
    value = GetObfuscatedValue(value, kVisibleSuffixLength);
  }

  MemoryDataType memory_data_type =
      AttributeTypeToMemoryDataType(primary_attribute_type);
  MemorySearchResult entry(
      memory_data_type, GetMemoryDataTypeNameForI18n(memory_data_type),
      std::move(value),
      CalculateRankingScore(entity.use_count(), entity.use_date()));
  entry.is_obfuscated = primary_attribute_type.is_obfuscated();
  entry.identifier = *entity.guid();
  entry.metadata_list = GetMetadataFromEntityAttributes(entity, app_locale,
                                                        primary_attribute_type);
  entry.is_local = [&] {
    switch (entity.record_type()) {
      case EntityInstance::RecordType::kLocal:
        return true;
      case EntityInstance::RecordType::kServerWallet:
      case EntityInstance::RecordType::kPersonalContext:
        return false;
    }
    NOTREACHED();
  }();
  if (base::optional_ref<const AttributeInstance> attribute =
          entity.attribute(primary_attribute_type)) {
    entry.typed_value = GetAttributeTypedValue(*attribute);
  }
  return entry;
}

// Creates a data entry from an address profile for a specific field type.
MemorySearchResult CreateResultFromAddressProfile(
    const AutofillProfile& profile,
    std::u16string value,
    MemoryDataType memory_data_type,
    FieldType field_type,
    const std::string& app_locale) {
  MemorySearchResult entry = MemorySearchResult(
      memory_data_type, GetMemoryDataTypeNameForI18n(memory_data_type),
      std::move(value), profile.GetRankingScore(base::Time::Now()));

  entry.identifier = profile.guid();
  entry.is_local = [&] {
    switch (profile.record_type()) {
      case AutofillProfile::RecordType::kLocalOrSyncable:
      case AutofillProfile::RecordType::kAccount:
        return true;
      case AutofillProfile::RecordType::kAccountHome:
      case AutofillProfile::RecordType::kAccountWork:
      case AutofillProfile::RecordType::kAccountNameEmail:
        return false;
    }
    NOTREACHED();
  }();

  // Add other address fields as metadata.
  AddMetadataToResult(entry, profile, MemoryDataType::kNameFull, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, MemoryDataType::kAddressStreetAddress,
                      field_type, app_locale);
  AddMetadataToResult(entry, profile, MemoryDataType::kAddressCity, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, MemoryDataType::kAddressState, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, MemoryDataType::kAddressZip, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, MemoryDataType::kAddressCountry,
                      field_type, app_locale);

  if (std::optional<TypedValue> typed_home_country =
          GetHomeCountryTypedValue(profile)) {
    if (auto it = std::ranges::find(entry.metadata_list,
                                    MemoryDataType::kAddressCountry,
                                    &EntryMetadata::type);
        it != entry.metadata_list.end()) {
      it->typed_value = typed_home_country;
    }
    if (memory_data_type == MemoryDataType::kAddressCountry) {
      entry.typed_value = std::move(typed_home_country);
    }
  }

  entry.confidence_score = profile.GetRankingScore(base::Time::Now());
  return entry;
}

// Fetches data for a specific field type from all available address profiles.
std::vector<MemorySearchResult> FetchDataFromAddressProfiles(
    const PersonalDataManager& personal_data_manager,
    FieldType field_type,
    MemoryDataType memory_data_type) {
  std::vector<MemorySearchResult> entries;
  std::string app_locale =
      personal_data_manager.address_data_manager().app_locale();

  for (const AutofillProfile* profile :
       personal_data_manager.address_data_manager().GetProfiles()) {
    std::u16string value = profile->GetInfo(field_type, app_locale);
    if (value.empty()) {
      continue;
    }

    entries.push_back(CreateResultFromAddressProfile(
        *profile, std::move(value), memory_data_type, field_type, app_locale));
  }
  return entries;
}

// Fetches full address representation from all profiles.
std::vector<MemorySearchResult> FetchFullAddressData(
    const PersonalDataManager& personal_data_manager) {
  std::vector<MemorySearchResult> entries;
  std::string app_locale =
      personal_data_manager.address_data_manager().app_locale();
  for (const AutofillProfile* profile :
       personal_data_manager.address_data_manager().GetProfiles()) {
    // Profiles that don't have at least a street address are not useful for
    // full address suggestions (e.g. profiles with only name, email, and
    // country).
    if (!profile->HasRawInfo(ADDRESS_HOME_STREET_ADDRESS)) {
      continue;
    }

    std::u16string full_address = GetEnvelopeStyleAddress(
        *profile, app_locale, /*include_recipient=*/false,
        /*include_country=*/true);
    if (full_address.empty()) {
      continue;
    }

    std::u16string separator =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
    base::ReplaceChars(full_address, u"\n", separator, &full_address);

    entries.push_back(CreateResultFromAddressProfile(
        *profile, std::move(full_address), MemoryDataType::kAddressFull,
        ADDRESS_HOME_ADDRESS, app_locale));
  }
  return entries;
}

// Fetches data from EntityDataManager (Autofill AI) for the requested
// attribute.
std::vector<MemorySearchResult> FetchAutofillAiAttributeData(
    const EntityDataManager* entity_data_manager,
    AttributeType attribute_type,
    std::string_view app_locale) {
  std::vector<MemorySearchResult> entries;
  if (!entity_data_manager) {
    return entries;
  }
  entries.reserve(entity_data_manager->GetEntityInstances().size());
  for (const EntityInstance& entity :
       entity_data_manager->GetEntityInstances()) {
    if (entity.type() != attribute_type.entity_type()) {
      continue;
    }
    base::optional_ref<const AttributeInstance> attr =
        entity.attribute(attribute_type);
    if (!attr) {
      continue;
    }

    std::u16string attr_value = attr->GetCompleteInfo(app_locale);
    if (attr_value.empty()) {
      continue;
    }

    entries.push_back(CreateMemorySearchResultForEntity(
        entity, attr->type(), std::move(attr_value), app_locale));
  }
  return entries;
}

}  // namespace

AutofillDataProvider::AutofillDataProvider(
    const PersonalDataManager* personal_data_manager,
    const EntityDataManager* entity_data_manager)
    : personal_data_manager_(personal_data_manager),
      entity_data_manager_(entity_data_manager) {}

AutofillDataProvider::~AutofillDataProvider() = default;

void AutofillDataProvider::RetrieveAll(
    const std::vector<MemoryDataType>& types,
    base::OnceCallback<void(std::vector<MemorySearchResult>)> callback) {
  std::vector<MemorySearchResult> combined_results;
  for (MemoryDataType memory_data_type : types) {
    if (memory_data_type == MemoryDataType::kUnknown) {
      continue;
    }
    base::Extend(combined_results, GetAutofillData(memory_data_type));
  }
  std::move(callback).Run(std::move(combined_results));
}

std::vector<MemorySearchResult> AutofillDataProvider::GetAutofillData(
    MemoryDataType memory_data_type) {
  if (!personal_data_manager_) {
    return {};
  }
  std::vector<MemorySearchResult> entries;
  switch (GetMemoryDataTypeCategory(memory_data_type)) {
    case MemoryDataTypeCategory::kContactInfo: {
      if (memory_data_type == MemoryDataType::kAddressFull) {
        entries = FetchFullAddressData(*personal_data_manager_);
      } else {
        std::optional<FieldType> field_type = ToFieldType(memory_data_type);
        if (field_type) {
          entries = FetchDataFromAddressProfiles(*personal_data_manager_,
                                                 *field_type, memory_data_type);
        }
      }
      break;
    }
    case MemoryDataTypeCategory::kCreditCard: {
      std::optional<FieldType> field_type = ToFieldType(memory_data_type);
      if (field_type) {
        entries = FetchCreditCardData(*field_type, memory_data_type);
      }
      break;
    }
    case MemoryDataTypeCategory::kIban: {
      entries = FetchIbanData();
      break;
    }
    case MemoryDataTypeCategory::kPassport:
    case MemoryDataTypeCategory::kDriversLicense:
    case MemoryDataTypeCategory::kNationalIdCard:
    case MemoryDataTypeCategory::kFlightReservation:
    case MemoryDataTypeCategory::kKnownTravelerNumber:
    case MemoryDataTypeCategory::kRedressNumber:
    case MemoryDataTypeCategory::kVehicle:
    case MemoryDataTypeCategory::kOrder:
    case MemoryDataTypeCategory::kShipment: {
      std::optional<AttributeType> attribute_type =
          ToAttributeType(memory_data_type);
      if (attribute_type) {
        entries = FetchAutofillAiAttributeData(
            entity_data_manager_, *attribute_type,
            personal_data_manager_->address_data_manager().app_locale());
      }
      break;
    }
    case MemoryDataTypeCategory::kUnknown:
      break;
  }

  std::ranges::stable_sort(
      entries, [](const MemorySearchResult& a, const MemorySearchResult& b) {
        return a.confidence_score > b.confidence_score;
      });

  for (MemorySearchResult& entry : entries) {
    entry.sources.emplace_back(MemoryEntrySourceType::kAutofill);
  }
  return entries;
}

// Fetches IBAN data from PersonalDataManager.
std::vector<MemorySearchResult> AutofillDataProvider::FetchIbanData() {
  std::vector<MemorySearchResult> entries;
  for (const Iban* iban :
       personal_data_manager_->payments_data_manager().GetIbans()) {
    std::u16string obfuscated_value =
        iban->GetIdentifierStringForAutofillDisplay();
    MemorySearchResult entry(
        MemoryDataType::kIban,
        GetMemoryDataTypeNameForI18n(MemoryDataType::kIban), obfuscated_value,
        iban->usage_history().GetRankingScore(base::Time::Now()));
    entry.is_obfuscated = true;
    entry.is_local = [&] {
      switch (iban->record_type()) {
        case Iban::kLocalIban:
          return true;
        case Iban::kServerIban:
        case Iban::kUnknown:
          return false;
      }
      NOTREACHED();
    }();
    switch (iban->record_type()) {
      case Iban::kLocalIban:
        entry.identifier = iban->guid();
        break;
      default:
        entry.identifier = iban->instrument_id();
        break;
    }

    if (!iban->nickname().empty()) {
      entry.metadata_list.emplace_back(
          MemoryDataType::kIbanNickname,
          GetMemoryDataTypeNameForI18n(MemoryDataType::kIbanNickname),
          std::u16string(iban->nickname()));
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::vector<MemorySearchResult> AutofillDataProvider::FetchCreditCardData(
    FieldType field_type,
    MemoryDataType memory_data_type) {
  std::vector<MemorySearchResult> entries;
  for (const CreditCard* credit_card : GetCreditCardsToSuggest(
           personal_data_manager_->payments_data_manager())) {
    std::u16string value = credit_card->GetInfo(
        field_type,
        personal_data_manager_->address_data_manager().app_locale());
    if (value.empty()) {
      continue;
    }

    if (field_type == CREDIT_CARD_NUMBER) {
      value = credit_card->ObfuscatedNumberWithVisibleLastFourDigits();
    } else if (field_type == CREDIT_CARD_VERIFICATION_CODE) {
      value =
          CreditCard::GetMidlineEllipsisPlainDots(credit_card->cvc().length());
    }

    MemorySearchResult entry(
        memory_data_type, GetMemoryDataTypeNameForI18n(memory_data_type),
        std::move(value),
        credit_card->usage_history().GetRankingScore(base::Time::Now()));
    entry.is_obfuscated = (field_type == CREDIT_CARD_NUMBER ||
                           field_type == CREDIT_CARD_VERIFICATION_CODE);
    entry.identifier = credit_card->guid();
    entry.is_local = [&] {
      switch (credit_card->record_type()) {
        case CreditCard::RecordType::kLocalCard:
          return true;
        case CreditCard::RecordType::kMaskedServerCard:
        case CreditCard::RecordType::kFullServerCard:
        case CreditCard::RecordType::kVirtualCard:
          return false;
      }
      NOTREACHED();
    }();

    std::string app_locale =
        personal_data_manager_->address_data_manager().app_locale();

    // All of the non-empty types different than the one being requested are
    // added as metadata.
    if (memory_data_type != MemoryDataType::kCreditCardNumber) {
      std::u16string card_number =
          credit_card->ObfuscatedNumberWithVisibleLastFourDigits();
      if (!card_number.empty()) {
        entry.metadata_list.emplace_back(
            MemoryDataType::kCreditCardNumber,
            GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNumber),
            std::move(card_number));
      }
    }
    if (memory_data_type != MemoryDataType::kCreditCardSecurityCode &&
        !credit_card->cvc().empty()) {
      entry.metadata_list.emplace_back(
          MemoryDataType::kCreditCardSecurityCode,
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardSecurityCode),
          CreditCard::GetMidlineEllipsisPlainDots(credit_card->cvc().length()));
    }

    AddMetadataToResult(entry, *credit_card,
                        MemoryDataType::kCreditCardNameOnCard, field_type,
                        app_locale);
    AddMetadataToResult(entry, *credit_card,
                        MemoryDataType::kCreditCardExpirationDate, field_type,
                        app_locale);
    if (!credit_card->nickname().empty()) {
      entry.metadata_list.emplace_back(
          MemoryDataType::kCreditCardNickname,
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNickname),
          std::u16string(credit_card->nickname()));
    }

    if (std::optional<TypedValue> typed_value =
            GetExpirationDateTypedValue(*credit_card)) {
      if (auto it = std::ranges::find(entry.metadata_list,
                                      MemoryDataType::kCreditCardExpirationDate,
                                      &EntryMetadata::type);
          it != entry.metadata_list.end()) {
        it->typed_value = typed_value;
      }
      if (memory_data_type == MemoryDataType::kCreditCardExpirationDate) {
        entry.typed_value = std::move(typed_value);
      }
    }

    entries.push_back(std::move(entry));
  }

  return entries;
}

}  // namespace autofill
