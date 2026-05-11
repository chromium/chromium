// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_provider_impl.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/extend.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/data_model/form_group.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::accessibility_annotator::EntryMetadata;
using ::accessibility_annotator::EntryType;
using ::accessibility_annotator::MemoryEntrySource;
using ::accessibility_annotator::MemoryEntrySourceType;
using ::accessibility_annotator::MemorySearchResult;

// Adds metadata from `form_group` to `entry` if `metadata_entry_type` maps to a
// `FieldType` and differs from `primary_field_type`.
void AddMetadataToResult(MemorySearchResult& entry,
                         const FormGroup& form_group,
                         EntryType metadata_entry_type,
                         FieldType primary_field_type,
                         const std::string& app_locale) {
  std::optional<AtMemoryDataType> data_type =
      ToAtMemoryDataType(metadata_entry_type);
  if (!data_type || !std::holds_alternative<FieldType>(*data_type)) {
    return;
  }
  FieldType other_field_type = std::get<FieldType>(*data_type);
  if (other_field_type == primary_field_type) {
    return;
  }
  std::u16string metadata_value =
      form_group.GetInfo(other_field_type, app_locale);
  if (!metadata_value.empty()) {
    entry.metadata_list.emplace_back(
        metadata_entry_type, GetEntryTypeNameForI18n(metadata_entry_type),
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

// Evaluates the primary value of a result for the given entity instance.
std::u16string GetFormattedEntityValue(const EntityInstance& entity,
                                       std::string_view app_locale) {
  // TODO(crbug.com/492978632): Handle fetching the unmasked data for server
  // EntityInstances.
  auto get_primary_attribute_name =
      [](EntityTypeName entity_type_name) -> AttributeTypeName {
    switch (entity_type_name) {
      case EntityTypeName::kVehicle:
        return AttributeTypeName::kVehiclePlateNumber;
      case EntityTypeName::kPassport:
        return AttributeTypeName::kPassportNumber;
      case EntityTypeName::kDriversLicense:
        return AttributeTypeName::kDriversLicenseNumber;
      case EntityTypeName::kOrder:
        return AttributeTypeName::kOrderId;
      case EntityTypeName::kNationalIdCard:
        return AttributeTypeName::kNationalIdCardNumber;
      case EntityTypeName::kKnownTravelerNumber:
        return AttributeTypeName::kKnownTravelerNumberNumber;
      case EntityTypeName::kRedressNumber:
        return AttributeTypeName::kRedressNumberNumber;
      case EntityTypeName::kFlightReservation:
        return AttributeTypeName::kFlightReservationFlightNumber;
      case EntityTypeName::kShipment:
        return AttributeTypeName::kShipmentTrackingNumber;
    }
  };

  base::optional_ref<const AttributeInstance> primary_attr = entity.attribute(
      AttributeType(get_primary_attribute_name(entity.type().name())));
  if (primary_attr) {
    const std::u16string value = primary_attr->GetCompleteInfo(app_locale);
    if (!value.empty()) {
      return value;
    }
  }

  // Fallback to the first non-empty attribute if no primary attribute is found.
  for (const AttributeInstance& attribute : entity.attributes()) {
    const std::u16string attr_value = attribute.GetCompleteInfo(app_locale);
    if (!attr_value.empty()) {
      return attr_value;
    }
  }

  return std::u16string();
}

// Creates a data entry for a specific attribute of an Autofill AI entity.
MemorySearchResult CreateResultFromEntityAttribute(
    const EntityInstance& entity,
    const AttributeInstance& attr,
    EntryType entry_type,
    std::string_view app_locale) {
  CHECK_EQ(entity.type(), attr.type().entity_type());
  MemorySearchResult entry = MemorySearchResult(
      entry_type, GetEntryTypeNameForI18n(entry_type),
      attr.GetCompleteInfo(app_locale),
      CalculateRankingScore(entity.use_count(), entity.use_date()));

  // Add all other non-empty attributes as metadata.
  for (const AttributeInstance& other_attr : entity.attributes()) {
    if (other_attr.type() == attr.type()) {
      continue;
    }
    std::u16string other_value = other_attr.GetCompleteInfo(app_locale);
    if (!other_value.empty()) {
      EntryType metadata_type = AttributeTypeToEntryType(other_attr.type());
      entry.metadata_list.emplace_back(metadata_type,
                                       GetEntryTypeNameForI18n(metadata_type),
                                       std::move(other_value));
    }
  }

  return entry;
}

// Creates a data entry from an address profile for a specific field type.
MemorySearchResult CreateResultFromAddressProfile(
    const AutofillProfile& profile,
    std::u16string value,
    EntryType entry_type,
    FieldType field_type,
    const std::string& app_locale) {
  MemorySearchResult entry = MemorySearchResult(
      entry_type, GetEntryTypeNameForI18n(entry_type), std::move(value),
      profile.GetRankingScore(base::Time::Now()));

  // Add other address fields as metadata.
  AddMetadataToResult(entry, profile, EntryType::kNameFull, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, EntryType::kAddressStreetAddress,
                      field_type, app_locale);
  AddMetadataToResult(entry, profile, EntryType::kAddressCity, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, EntryType::kAddressState, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, EntryType::kAddressZip, field_type,
                      app_locale);
  AddMetadataToResult(entry, profile, EntryType::kAddressCountry, field_type,
                      app_locale);

  entry.confidence_score = profile.GetRankingScore(base::Time::Now());
  return entry;
}

// Fetches data for a specific field type from all available address profiles.
std::vector<MemorySearchResult> FetchDataFromAddressProfiles(
    const PersonalDataManager& personal_data_manager,
    FieldType field_type,
    EntryType entry_type) {
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
        *profile, std::move(value), entry_type, field_type, app_locale));
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
        *profile, std::move(full_address), EntryType::kAddressFull,
        ADDRESS_HOME_ADDRESS, app_locale));
  }
  return entries;
}

// Fetches data from EntityDataManager (Autofill AI) for the requested entity.
std::vector<MemorySearchResult> FetchAutofillAiEntityData(
    const EntityDataManager* entity_data_manager,
    EntryType entry_type,
    EntityType entity_type,
    std::string_view app_locale) {
  std::vector<MemorySearchResult> entries;
  if (!entity_data_manager) {
    return entries;
  }
  for (const EntityInstance& entity :
       entity_data_manager->GetEntityInstances()) {
    if (entity.type() != entity_type) {
      continue;
    }

    // TODO(crbug.com/492978632): Handle masking SPII data for Autofill AI.
    std::vector<EntryMetadata> all_metadata;
    for (const AttributeInstance& attr : entity.attributes()) {
      std::u16string attr_value = attr.GetCompleteInfo(app_locale);
      if (!attr_value.empty()) {
        EntryType metadata_type = AttributeTypeToEntryType(attr.type());
        all_metadata.emplace_back(
            metadata_type, GetEntryTypeNameForI18n(metadata_type), attr_value);
      }
    }

    std::u16string value = GetFormattedEntityValue(entity, app_locale);
    if (!value.empty()) {
      MemorySearchResult entry = MemorySearchResult(
          entry_type, GetEntryTypeNameForI18n(entry_type), std::move(value),
          CalculateRankingScore(entity.use_count(), entity.use_date()));
      entry.metadata_list = std::move(all_metadata);
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}

// Fetches data from EntityDataManager (Autofill AI) for the requested
// attribute.
std::vector<MemorySearchResult> FetchAutofillAiAttributeData(
    const EntityDataManager* entity_data_manager,
    EntryType entry_type,
    AttributeType attribute_type,
    std::string_view app_locale) {
  std::vector<MemorySearchResult> entries;
  if (!entity_data_manager) {
    return entries;
  }
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

    entries.push_back(
        CreateResultFromEntityAttribute(entity, *attr, entry_type, app_locale));
  }
  return entries;
}

}  // namespace

AutofillDataProviderImpl::AutofillDataProviderImpl(
    const PersonalDataManager* personal_data_manager,
    const EntityDataManager* entity_data_manager)
    : personal_data_manager_(personal_data_manager),
      entity_data_manager_(entity_data_manager) {}

AutofillDataProviderImpl::~AutofillDataProviderImpl() = default;

std::string_view AutofillDataProviderImpl::GetHistogramSuffix() const {
  return "AutofillDataProvider";
}

void AutofillDataProviderImpl::RetrieveAll(
    EntryType entry_type,
    base::OnceCallback<void(std::vector<MemorySearchResult>)> callback) {
  std::optional<AtMemoryDataType> at_memory_type =
      ToAtMemoryDataType(entry_type);
  if (!at_memory_type) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(GetAutofillData(entry_type, *at_memory_type));
}

std::vector<MemorySearchResult> AutofillDataProviderImpl::GetAutofillData(
    EntryType entry_type,
    AtMemoryDataType at_memory_type) {
  if (!personal_data_manager_) {
    return {};
  }
  std::vector<MemorySearchResult> entries = std::visit(
      absl::Overload{
          [this, entry_type](
              FieldType field_type) -> std::vector<MemorySearchResult> {
            if (field_type == IBAN_VALUE) {
              return FetchIbanData();
            }
            if (field_type == ADDRESS_HOME_ADDRESS) {
              return FetchFullAddressData(*personal_data_manager_);
            }
            if (GroupTypeOfFieldType(field_type) ==
                FieldTypeGroup::kCreditCard) {
              return FetchCreditCardData(field_type, entry_type);
            }
            return FetchDataFromAddressProfiles(*personal_data_manager_,
                                                field_type, entry_type);
          },
          [this, entry_type](
              EntityType entity_type) -> std::vector<MemorySearchResult> {
            return FetchAutofillAiEntityData(
                entity_data_manager_, entry_type, entity_type,
                personal_data_manager_->address_data_manager().app_locale());
          },
          [this, entry_type](
              AttributeType attribute_type) -> std::vector<MemorySearchResult> {
            return FetchAutofillAiAttributeData(
                entity_data_manager_, entry_type, attribute_type,
                personal_data_manager_->address_data_manager().app_locale());
          },
      },
      at_memory_type);

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
std::vector<MemorySearchResult> AutofillDataProviderImpl::FetchIbanData() {
  std::vector<MemorySearchResult> entries;
  for (const Iban* iban :
       personal_data_manager_->payments_data_manager().GetIbans()) {
    std::u16string obfuscated_value =
        iban->GetIdentifierStringForAutofillDisplay();
    MemorySearchResult entry(
        EntryType::kIban, GetEntryTypeNameForI18n(EntryType::kIban),
        obfuscated_value,
        iban->usage_history().GetRankingScore(base::Time::Now()));
    entry.is_obfuscated = true;
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
          EntryType::kIbanNickname,
          GetEntryTypeNameForI18n(EntryType::kIbanNickname),
          std::u16string(iban->nickname()));
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::vector<MemorySearchResult> AutofillDataProviderImpl::FetchCreditCardData(
    FieldType field_type,
    EntryType entry_type) {
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
      value = std::u16string(3, kMidlineEllipsisPlainDot);
    }

    // TODO(crbug.com/497795513): Set `is_obfuscated` and `reveal_callback` for
    // credit card number and CVV and use it to reveal the number after re-auth.
    MemorySearchResult entry(
        entry_type, GetEntryTypeNameForI18n(entry_type), std::move(value),
        credit_card->usage_history().GetRankingScore(base::Time::Now()));
    entry.identifier = credit_card->guid();

    std::string app_locale =
        personal_data_manager_->address_data_manager().app_locale();

    // All of the types different than the one being requested are added as
    // metadata.
    if (entry_type != EntryType::kCreditCardNumber) {
      entry.metadata_list.emplace_back(
          EntryType::kCreditCardNumber,
          GetEntryTypeNameForI18n(EntryType::kCreditCardNumber),
          credit_card->ObfuscatedNumberWithVisibleLastFourDigits());
    }
    if (entry_type != EntryType::kCreditCardSecurityCode) {
      entry.metadata_list.emplace_back(
          EntryType::kCreditCardSecurityCode,
          GetEntryTypeNameForI18n(EntryType::kCreditCardSecurityCode),
          std::u16string(3, kMidlineEllipsisPlainDot));
    }

    AddMetadataToResult(entry, *credit_card, EntryType::kCreditCardNameOnCard,
                        field_type, app_locale);
    AddMetadataToResult(entry, *credit_card,
                        EntryType::kCreditCardExpirationDate, field_type,
                        app_locale);
    if (!credit_card->nickname().empty()) {
      entry.metadata_list.emplace_back(
          EntryType::kCreditCardNickname,
          GetEntryTypeNameForI18n(EntryType::kCreditCardNickname),
          std::u16string(credit_card->nickname()));
    }

    entries.push_back(std::move(entry));
  }

  return entries;
}

}  // namespace autofill
