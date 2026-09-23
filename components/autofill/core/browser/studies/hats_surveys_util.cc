// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/studies/hats_surveys_util.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {

// Returns true if all preconditions for the `filling_product`-specific survey
// are fulfilled.
bool CanTriggerPersonalizationAndTrustSurvey(const FormStructure& form,
                                             FillingProduct filling_product) {
  auto is_survey_enabled = [](FillingProduct filling_product) {
    switch (filling_product) {
      case FillingProduct::kAddress:
        return base::FeatureList::IsEnabled(
            features::kAutofillPersonalizationAndTrustAddressSurvey);
      case FillingProduct::kAutofillAi:
        return base::FeatureList::IsEnabled(
            features::kAutofillPersonalizationAndTrustAutofillAiSurvey);
      case FillingProduct::kNone:
      case FillingProduct::kCreditCard:
      case FillingProduct::kMerchantPromoCode:
      case FillingProduct::kIban:
      case FillingProduct::kAutocomplete:
      case FillingProduct::kPassword:
      case FillingProduct::kCompose:
      case FillingProduct::kLoyaltyCard:
      case FillingProduct::kIdentityCredential:
      case FillingProduct::kDataList:
      case FillingProduct::kOneTimePassword:
      case FillingProduct::kPasskey:
      case FillingProduct::kAtMemory:
        NOTIMPLEMENTED();
        break;
    }
    return false;
  };
  auto count_filled_fields_for_filling_product =
      [](const FormStructure& form, FillingProduct filling_product) -> size_t {
    return std::ranges::count_if(
        form.fields(),
        [filling_product](const std::unique_ptr<AutofillField>& field) {
          return field->filling_product() == filling_product;
        });
  };
  auto minimum_field_count_requirement =
      [](FillingProduct filling_product) -> size_t {
    switch (filling_product) {
      case FillingProduct::kAddress:
        return kMinRequiredFieldsForHeuristics;
      case FillingProduct::kAutofillAi:
        return 1;
      case FillingProduct::kNone:
      case FillingProduct::kCreditCard:
      case FillingProduct::kMerchantPromoCode:
      case FillingProduct::kIban:
      case FillingProduct::kAutocomplete:
      case FillingProduct::kPassword:
      case FillingProduct::kCompose:
      case FillingProduct::kLoyaltyCard:
      case FillingProduct::kIdentityCredential:
      case FillingProduct::kDataList:
      case FillingProduct::kOneTimePassword:
      case FillingProduct::kPasskey:
      case FillingProduct::kAtMemory:
        NOTIMPLEMENTED();
        break;
    }
    return std::numeric_limits<size_t>::max();
  };

  return count_filled_fields_for_filling_product(form, filling_product) >=
             minimum_field_count_requirement(filling_product) &&
         is_survey_enabled(filling_product);
}

template <typename Range, typename Proj>
std::string ListToString(Range&& range, Proj&& projection) {
  return base::JoinString(base::ToVector(std::forward<Range>(range),
                                         std::forward<Proj>(projection)),
                          "; ");
}

// Creates the product specific string data map for the personalization and
// trust surveys.
HatsSurveyStringData CollectPersonalizationAndTrustFillingData(
    const FormStructure& submitted_form,
    const AutofillAiManager* autofill_ai_manager) {
  size_t num_correctly_filled = 0;
  size_t num_submitted_empty = 0;
  size_t num_modified_after_filling = 0;
  size_t num_cleared_after_filling = 0;
  size_t num_manually_filled = 0;
  FieldTypeSet all_field_types;
  FillingProductSet filling_products_used;

  for (const std::unique_ptr<AutofillField>& field : submitted_form) {
    all_field_types.insert_all(field->Type().GetTypes());
    if (field->filling_product() != FillingProduct::kNone) {
      filling_products_used.insert(field->filling_product());
    }

    const autofill_metrics::FieldFillingStatus status =
        autofill_metrics::GetFieldFillingStatus(*field);
    switch (status) {
      case autofill_metrics::FieldFillingStatus::kAccepted:
        ++num_correctly_filled;
        break;
      case autofill_metrics::FieldFillingStatus::kLeftEmpty:
        ++num_submitted_empty;
        break;
      case autofill_metrics::FieldFillingStatus::kCorrectedToSameType:
      case autofill_metrics::FieldFillingStatus::kCorrectedToDifferentType:
      case autofill_metrics::FieldFillingStatus::kCorrectedToUnknownType:
        ++num_modified_after_filling;
        break;
      case autofill_metrics::FieldFillingStatus::kCorrectedToEmpty:
        ++num_cleared_after_filling;
        break;
      case autofill_metrics::FieldFillingStatus::kManuallyFilledToSameType:
      case autofill_metrics::FieldFillingStatus::kManuallyFilledToDifferentType:
      case autofill_metrics::FieldFillingStatus::kManuallyFilledToUnknownType:
        ++num_manually_filled;
        break;
    }
  }

  std::vector<EntityInstance::RecordType> autofill_ai_record_types_used;
  std::vector<EntityType> autofill_ai_entity_types_used;
  if (autofill_ai_manager) {
    std::optional<RecentUserAutofillAiInteractionsForHats::InteractionDetails>
        interaction = autofill_ai_manager->GetRecentUserInteractionForHats(
            submitted_form.global_id());
    if (interaction) {
      autofill_ai_record_types_used =
          std::move(interaction->accepted_entity_record_type);
      autofill_ai_entity_types_used =
          std::move(interaction->entity_type_accepted);
    }
  }

  return {{"All field types", FieldTypeSetToString(all_field_types)},
          {"Total number of fields in form",
           base::NumberToString(submitted_form.field_count())},
          {"Number of correctly filled fields",
           base::NumberToString(num_correctly_filled)},
          {"Number of fields that were submitted empty without filling",
           base::NumberToString(num_submitted_empty)},
          {"Number of fields that were modified after filling",
           base::NumberToString(num_modified_after_filling)},
          {"Number of fields that were cleared after filling",
           base::NumberToString(num_cleared_after_filling)},
          {"Number of fields that were manually filled without filling",
           base::NumberToString(num_manually_filled)},
          {"Filling products used",
           base::JoinString(
               base::ToVector(filling_products_used, &FillingProductToString),
               ", ")},
          {"AutofillAi entity record types used",
           ListToString(autofill_ai_record_types_used,
                        &EntityRecordTypeToMetricsString)},
          {"AutofillAi entity types used",
           ListToString(autofill_ai_entity_types_used,
                        &EntityType::name_as_string)},
          {"Time since last Autofill use",
           submitted_form.last_filling_timestamp()
               .transform([](base::TimeTicks time) {
                 return base::NumberToString(
                     std::max(base::TimeDelta(), base::TimeTicks::Now() - time)
                         .InSeconds());
               })
               .value_or("")}};
}

}  // namespace

void MaybeTriggerFormSubmissionHatsSurveys(
    AutofillClient& client,
    const FormStructure& submitted_form) {
  for (FillingProduct filling_product :
       std::array{FillingProduct::kAutofillAi, FillingProduct::kAddress}) {
    if (CanTriggerPersonalizationAndTrustSurvey(submitted_form,
                                                filling_product)) {
      // Product was used on at least one field in the submitted form, initiate
      // attempt to show survey. To prevent timing-dependent behavior when
      // triggering multiple surveys, return after first attempt to start a
      // survey. For this reason, try to trigger rarely used products first.
      client.TriggerPersonalizationAndTrustSurveys(
          filling_product, CollectPersonalizationAndTrustFillingData(
                               submitted_form, client.GetAutofillAiManager()));
      return;
    }
  }
}

RecentUserAutofillAiInteractionsForHats::
    RecentUserAutofillAiInteractionsForHats() = default;
RecentUserAutofillAiInteractionsForHats::
    ~RecentUserAutofillAiInteractionsForHats() = default;

void RecentUserAutofillAiInteractionsForHats::SuggestionsShown(
    const FormStructure& form,
    const AutofillField& field) {
  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  // Do not overwrite cases in which a suggestion was previously accepted.
  if (it == user_suggestion_interactions_per_form_.end() ||
      it->second.entity_type_accepted.empty()) {
    user_suggestion_interactions_per_form_.Put(
        form.global_id(),
        InteractionDetails{
            .entity_type_accepted = {},
            .accepted_entity_record_type = {},
            .autofill_ai_field_types = field.Type().GetAutofillAiTypes(),
            .is_filled_per_field = std::vector<bool>(form.field_count(), false),
        });
  }
}

void RecentUserAutofillAiInteractionsForHats::SuggestionAccepted(
    const FormStructure& form,
    base::span<const AutofillField* const> filled_fields,
    const EntityInstance& entity) {
  const std::vector<bool> is_filled_per_field =
      base::ToVector(form, [&](const std::unique_ptr<AutofillField>& field) {
        return std::ranges::contains(filled_fields, field.get());
      });

  auto it = user_suggestion_interactions_per_form_.Get(form.global_id());
  if (it != user_suggestion_interactions_per_form_.end()) {
    it->second.entity_type_accepted.push_back(entity.type());
    it->second.accepted_entity_record_type.push_back(entity.record_type());
    if (it->second.is_filled_per_field.size() != is_filled_per_field.size()) {
      // Form was modified between filling operations, reset filled fields.
      it->second.is_filled_per_field = is_filled_per_field;
    } else {
      it->second.is_filled_per_field = base::ToVector(
          std::views::zip(it->second.is_filled_per_field, is_filled_per_field),
          [](std::pair<bool, bool> is_filled) {
            return is_filled.first || is_filled.second;
          });
    }
  }
}

std::optional<RecentUserAutofillAiInteractionsForHats::InteractionDetails>
RecentUserAutofillAiInteractionsForHats::GetRecentUserInteraction(
    FormGlobalId form_id) const {
  if (auto it = user_suggestion_interactions_per_form_.Peek(form_id);
      it != user_suggestion_interactions_per_form_.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace autofill
