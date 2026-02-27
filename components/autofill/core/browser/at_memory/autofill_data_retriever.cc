// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_retriever.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "base/containers/extend.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/usage_history_information.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Calculates a ranking score for an entity, based on frequency and recency of
// use.
double CalculateRankingScore(int64_t use_count, base::Time use_date) {
  UsageHistoryInformation usage_history;
  usage_history.set_use_count(use_count);
  usage_history.set_use_date(use_date);
  return usage_history.GetRankingScore(base::Time::Now());
}

// Returns the value of a specific attribute.
std::optional<std::u16string> GetAutofillAiAttributeValue(
    const EntityInstance& entity,
    AttributeType type,
    std::string_view app_locale) {
  base::optional_ref<const AttributeInstance> attr = entity.attribute(type);
  return attr ? std::make_optional(attr->GetCompleteInfo(app_locale))
              : std::nullopt;
}

// Fetches data for a specific field type from all available address profiles.
std::vector<MemorySearchResult> FetchDataFromAddressProfiles(
    PersonalDataManager& personal_data_manager,
    FieldType type) {
  std::vector<MemorySearchResult> results;
  for (const AutofillProfile* profile :
       personal_data_manager.address_data_manager().GetProfiles()) {
    std::u16string description = profile->GetRawInfo(NAME_FULL);
    if (description.empty()) {
      description = profile->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS);
    }
    // TODO(crbug.com/481979475): Use internationalization for these strings.
    description = description.empty()
                      ? u"Address"
                      : base::StrCat({u"Address: ", description});

    std::u16string value = profile->GetRawInfo(type);
    if (value.empty()) {
      continue;
    }

    MemorySearchResult result;
    result.value = std::move(value);
    result.title = result.value;
    result.description = std::move(description);
    result.ranking_score = profile->GetRankingScore(base::Time::Now());
    results.push_back(std::move(result));
  }
  return results;
}

// Fetches full address representation from all profiles.
std::vector<MemorySearchResult> FetchFullAddressData(
    PersonalDataManager& personal_data_manager) {
  std::vector<MemorySearchResult> results;
  std::string app_locale =
      personal_data_manager.address_data_manager().app_locale();
  for (const AutofillProfile* profile :
       personal_data_manager.address_data_manager().GetProfiles()) {
    std::u16string full_address = GetEnvelopeStyleAddress(
        *profile, app_locale, /*include_recipient=*/false,
        /*include_country=*/true);
    if (full_address.empty()) {
      continue;
    }
    base::ReplaceChars(full_address, u"\n", u", ", &full_address);
    std::u16string description = profile->GetRawInfo(NAME_FULL);
    if (description.empty()) {
      description = profile->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS);
    }
    MemorySearchResult result;
    result.value = std::move(full_address);
    result.title = result.value;
    // TODO(crbug.com/481979475): Use internationalization for the strings.
    result.description = base::StrCat({u"Address: ", description});
    result.ranking_score = profile->GetRankingScore(base::Time::Now());
    results.push_back(std::move(result));
  }
  return results;
}

// Fetches data from EntityDataManager (Autofill AI) for the requested entity.
std::vector<MemorySearchResult> FetchAutofillAiEntityData(
    EntityDataManager* entity_data_manager,
    EntityType entity_type,
    std::string_view app_locale) {
  std::vector<MemorySearchResult> results;
  if (!entity_data_manager) {
    return results;
  }
  for (const EntityInstance& entity :
       entity_data_manager->GetEntityInstances()) {
    if (entity.type() != entity_type) {
      continue;
    }

    std::u16string description = entity.nickname().empty()
                                     ? entity.type().GetNameForI18n()
                                     : base::UTF8ToUTF16(entity.nickname());
    double ranking_score =
        CalculateRankingScore(entity.use_count(), entity.use_date());

    // Combined "Make Model" for generic vehicle intent.
    if (entity_type.name() == EntityTypeName::kVehicle) {
      std::optional<std::u16string> make = GetAutofillAiAttributeValue(
          entity, AttributeType(AttributeTypeName::kVehicleMake), app_locale);
      std::optional<std::u16string> model = GetAutofillAiAttributeValue(
          entity, AttributeType(AttributeTypeName::kVehicleModel), app_locale);
      if (make && model) {
        MemorySearchResult result;
        result.value = base::StrCat({*make, u" ", *model});
        result.title = result.value;
        result.description = description;
        result.ranking_score = ranking_score;
        results.push_back(std::move(result));
      }
    }

    for (const AttributeInstance& attr : entity.attributes()) {
      MemorySearchResult result;
      result.value = attr.GetCompleteInfo(app_locale);
      result.title = result.value;
      result.description =
          base::StrCat({description, u" - ", attr.type().GetNameForI18n()});
      result.ranking_score = ranking_score;
      results.push_back(std::move(result));
    }
  }
  return results;
}

// Fetches data from EntityDataManager (Autofill AI) for the requested
// attribute.
std::vector<MemorySearchResult> FetchAutofillAiAttributeData(
    EntityDataManager* entity_data_manager,
    AttributeType attribute_type,
    std::string_view app_locale) {
  std::vector<MemorySearchResult> results;
  if (!entity_data_manager) {
    return results;
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

    std::u16string description = entity.nickname().empty()
                                     ? entity.type().GetNameForI18n()
                                     : base::UTF8ToUTF16(entity.nickname());
    double ranking_score =
        CalculateRankingScore(entity.use_count(), entity.use_date());

    MemorySearchResult result;
    result.value = attr->GetCompleteInfo(app_locale);
    result.title = result.value;
    result.description =
        base::StrCat({description, u" - ", attr->type().GetNameForI18n()});
    result.ranking_score = ranking_score;
    results.push_back(std::move(result));
  }
  return results;
}

// Fetches IBAN data from PersonalDataManager.
std::vector<MemorySearchResult> FetchIbanData(
    PersonalDataManager& personal_data_manager) {
  std::vector<MemorySearchResult> results;
  for (const Iban* iban :
       personal_data_manager.payments_data_manager().GetIbans()) {
    MemorySearchResult result;
    result.value = iban->value();
    result.title = iban->GetIdentifierStringForAutofillDisplay();
    // TODO(crbug.com/481979475): Use internationalization for these strings.
    std::u16string nickname = iban->nickname();
    result.description =
        nickname.empty() ? u"IBAN" : base::StrCat({u"IBAN: ", nickname});
    result.ranking_score =
        iban->usage_history().GetRankingScore(base::Time::Now());
    results.push_back(std::move(result));
  }
  return results;
}

}  // namespace

AutofillDataRetriever::AutofillDataRetriever(AutofillClient& client)
    : client_(client) {}

AutofillDataRetriever::~AutofillDataRetriever() = default;

std::vector<MemorySearchResult> AutofillDataRetriever::RetrieveAll(
    AtMemoryDataType intent) {
  std::vector<MemorySearchResult> results = std::visit(
      absl::Overload{
          [this](FieldType field_type) -> std::vector<MemorySearchResult> {
            PersonalDataManager& pdm = client_->GetPersonalDataManager();
            if (field_type == IBAN_VALUE) {
              return FetchIbanData(pdm);
            }
            if (field_type == ADDRESS_HOME_ADDRESS) {
              std::vector<MemorySearchResult> results =
                  FetchDataFromAddressProfiles(pdm,
                                               ADDRESS_HOME_STREET_ADDRESS);
              base::Extend(results, FetchFullAddressData(pdm));
              return results;
            }
            return FetchDataFromAddressProfiles(pdm, field_type);
          },
          [this](EntityType entity_type) -> std::vector<MemorySearchResult> {
            return FetchAutofillAiEntityData(client_->GetEntityDataManager(),
                                             entity_type,
                                             client_->GetAppLocale());
          },
          [this](
              AttributeType attribute_type) -> std::vector<MemorySearchResult> {
            return FetchAutofillAiAttributeData(client_->GetEntityDataManager(),
                                                attribute_type,
                                                client_->GetAppLocale());
          },
      },
      intent);

  std::ranges::sort(
      results, [](const MemorySearchResult& a, const MemorySearchResult& b) {
        return a.ranking_score > b.ranking_score;
      });

  return results;
}

}  // namespace autofill
