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
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
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
  TypedValue typed_val = attribute.GetTypedValue();
  return typed_val.value_case() == TypedValue::VALUE_NOT_SET
             ? std::nullopt
             : std::optional(std::move(typed_val));
}

// Extracts the expiration date as a `TypedValue` if it is non-empty.
std::optional<TypedValue> GetExpirationDateTypedValue(const CreditCard& card) {
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
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (country_code.length() != 2) {
    return std::nullopt;
  }
  TypedValue typed_val;
  typed_val.set_country_code(country_code);
  return typed_val;
}

// Adds `value` of type `metadata_type` to `entry` as metadata, unless it is
// empty or `metadata_type` is the `primary_type` of `entry`.
void AddMetadataToResult(MemorySearchResult& entry,
                         MemoryDataType metadata_type,
                         MemoryDataType primary_type,
                         std::u16string value) {
  if (metadata_type == primary_type || value.empty()) {
    return;
  }
  entry.metadata_list.emplace_back(metadata_type,
                                   GetMemoryDataTypeNameForI18n(metadata_type),
                                   std::move(value));
}
// Same as `AddMetadataToResult()`, with the value read from `form_group`, if
// `metadata_type` maps to a `FieldType`.
void AddMetadataToResult(MemorySearchResult& entry,
                         const FormGroup& form_group,
                         MemoryDataType metadata_type,
                         MemoryDataType primary_type,
                         const std::string& app_locale) {
  if (std::optional<FieldType> field_type = ToFieldType(metadata_type)) {
    AddMetadataToResult(entry, metadata_type, primary_type,
                        form_group.GetInfo(*field_type, app_locale));
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
        return false;
      case EntityInstance::RecordType::kPersonalContext:
        NOTREACHED();
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

  // Add other profile fields as metadata.
  for (MemoryDataType metadata_type :
       {MemoryDataType::kNameFull, MemoryDataType::kAddressStreetAddress,
        MemoryDataType::kAddressCity, MemoryDataType::kAddressState,
        MemoryDataType::kAddressZip, MemoryDataType::kAddressCountry,
        MemoryDataType::kCompanyName, MemoryDataType::kEmail,
        MemoryDataType::kPhone}) {
    AddMetadataToResult(entry, profile, metadata_type, memory_data_type,
                        app_locale);
  }

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

  return entry;
}

// Fetches data of `memory_data_type` from all address profiles. For
// `kAddressFull`, the value is the envelope-style address.
std::vector<MemorySearchResult> FetchDataFromAddressProfiles(
    const PersonalDataManager& personal_data_manager,
    FieldType field_type,
    MemoryDataType memory_data_type) {
  std::vector<MemorySearchResult> entries;
  std::string app_locale =
      personal_data_manager.address_data_manager().app_locale();

  for (const AutofillProfile* profile :
       personal_data_manager.address_data_manager().GetProfiles()) {
    std::u16string value;
    if (memory_data_type == MemoryDataType::kAddressFull) {
      // Profiles without a street address (e.g. name, email and country only)
      // are not useful as full address suggestions.
      if (!profile->HasRawInfo(ADDRESS_HOME_STREET_ADDRESS)) {
        continue;
      }
      value = GetEnvelopeStyleAddress(*profile, app_locale,
                                      /*include_recipient=*/false,
                                      /*include_country=*/true);
      base::ReplaceChars(
          value, u"\n",
          l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR),
          &value);
    } else {
      value = profile->GetInfo(field_type, app_locale);
    }
    if (value.empty()) {
      continue;
    }
    entries.push_back(CreateResultFromAddressProfile(
        *profile, std::move(value), memory_data_type, app_locale));
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
    if (entity.type() != attribute_type.entity_type() ||
        entity.record_type() == EntityInstance::RecordType::kPersonalContext) {
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

// Extracts the string value for a given `type` from `loyalty_card`.
std::u16string GetLoyaltyCardAttributeValue(const LoyaltyCard& loyalty_card,
                                            MemoryDataType type) {
  if (type == MemoryDataType::kLoyaltyMembershipId) {
    return base::UTF8ToUTF16(loyalty_card.loyalty_card_number());
  }
  if (type == MemoryDataType::kLoyaltyMembershipProgram) {
    return base::UTF8ToUTF16(loyalty_card.program_name());
  }
  if (type == MemoryDataType::kLoyaltyMembershipProvider) {
    return base::UTF8ToUTF16(loyalty_card.merchant_name());
  }
  NOTREACHED();
}

// Creates a data entry from a loyalty card for a specific `memory_data_type`.
MemorySearchResult CreateResultFromLoyaltyCard(
    const LoyaltyCard& loyalty_card,
    std::u16string value,
    MemoryDataType memory_data_type) {
  MemorySearchResult entry(
      memory_data_type, GetMemoryDataTypeNameForI18n(memory_data_type),
      std::move(value),
      CalculateRankingScore(loyalty_card.use_count(), loyalty_card.use_date()));
  entry.identifier = *loyalty_card.id();
  entry.is_local = true;

  for (MemoryDataType other_type :
       {MemoryDataType::kLoyaltyMembershipId,
        MemoryDataType::kLoyaltyMembershipProgram,
        MemoryDataType::kLoyaltyMembershipProvider}) {
    if (other_type == memory_data_type) {
      continue;
    }
    std::u16string other_value =
        GetLoyaltyCardAttributeValue(loyalty_card, other_type);
    if (!other_value.empty()) {
      entry.metadata_list.emplace_back(other_type,
                                       GetMemoryDataTypeNameForI18n(other_type),
                                       std::move(other_value));
    }
  }
  return entry;
}

// Fetches loyalty card data from `valuables_data_manager`.
std::vector<MemorySearchResult> FetchLoyaltyCardData(
    const ValuablesDataManager* valuables_data_manager,
    MemoryDataType memory_data_type) {
  if (!valuables_data_manager) {
    return {};
  }

  std::vector<MemorySearchResult> entries;
  entries.reserve(valuables_data_manager->GetLoyaltyCards().size());
  for (const LoyaltyCard& loyalty_card :
       valuables_data_manager->GetLoyaltyCards()) {
    std::u16string value =
        GetLoyaltyCardAttributeValue(loyalty_card, memory_data_type);
    if (value.empty()) {
      continue;
    }
    entries.push_back(CreateResultFromLoyaltyCard(
        loyalty_card, std::move(value), memory_data_type));
  }
  return entries;
}

}  // namespace

AutofillDataProvider::AutofillDataProvider(
    const PersonalDataManager* personal_data_manager,
    const EntityDataManager* entity_data_manager,
    const ValuablesDataManager* valuables_data_manager)
    : personal_data_manager_(personal_data_manager),
      entity_data_manager_(entity_data_manager),
      valuables_data_manager_(valuables_data_manager) {}

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
      if (std::optional<FieldType> field_type = ToFieldType(memory_data_type)) {
        entries = FetchDataFromAddressProfiles(*personal_data_manager_,
                                               *field_type, memory_data_type);
      }
      break;
    }
    case MemoryDataTypeCategory::kCreditCard: {
      if (std::optional<FieldType> field_type = ToFieldType(memory_data_type)) {
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
      if (std::optional<AttributeType> attribute_type =
              ToAttributeType(memory_data_type)) {
        entries = FetchAutofillAiAttributeData(
            entity_data_manager_, *attribute_type,
            personal_data_manager_->address_data_manager().app_locale());
      }
      break;
    }
    case MemoryDataTypeCategory::kLoyaltyCard:
      entries = FetchLoyaltyCardData(valuables_data_manager_, memory_data_type);
      break;
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
    switch (iban->record_type()) {
      case Iban::kLocalIban:
        entry.is_local = true;
        entry.identifier = iban->guid();
        break;
      case Iban::kServerIban:
      case Iban::kUnknown:
        entry.is_local = false;
        entry.identifier = iban->instrument_id();
        break;
    }

    AddMetadataToResult(entry, MemoryDataType::kIbanNickname,
                        MemoryDataType::kIban, iban->nickname());
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::vector<MemorySearchResult> AutofillDataProvider::FetchCreditCardData(
    FieldType field_type,
    MemoryDataType memory_data_type) {
  std::vector<MemorySearchResult> entries;
  const std::string& app_locale =
      personal_data_manager_->address_data_manager().app_locale();
  for (const CreditCard* credit_card : GetCreditCardsToSuggest(
           personal_data_manager_->payments_data_manager())) {
    std::u16string value = credit_card->GetInfo(field_type, app_locale);
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

    // All of the non-empty types different than the one being requested are
    // added as metadata.
    AddMetadataToResult(
        entry, MemoryDataType::kCreditCardNumber, memory_data_type,
        credit_card->ObfuscatedNumberWithVisibleLastFourDigits());
    AddMetadataToResult(
        entry, MemoryDataType::kCreditCardSecurityCode, memory_data_type,
        CreditCard::GetMidlineEllipsisPlainDots(credit_card->cvc().length()));
    AddMetadataToResult(entry, *credit_card,
                        MemoryDataType::kCreditCardNameOnCard, memory_data_type,
                        app_locale);
    AddMetadataToResult(entry, *credit_card,
                        MemoryDataType::kCreditCardExpirationDate,
                        memory_data_type, app_locale);
    AddMetadataToResult(entry, MemoryDataType::kCreditCardNickname,
                        memory_data_type, credit_card->nickname());

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
