// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

namespace {

using autofill::FormFieldData;
using autofill::Suggestion;
using autofill::SuggestionType;
using PasswordFormClassification =
    autofill::AutofillClient::PasswordFormClassification;

// Returns `true` when we wish to offer plus address creation on a form with
// password manager classification `form_classification` and a focused field
// with id `focused_field_id`.
// If password manager did not recognize a username field or the username field
// is different from the focused field, this is always `true`. Otherwise,
// whether we offer plus address creation depends on the form type.
bool ShouldOfferPlusAddressCreationOnForm(
    const PasswordFormClassification& form_classification,
    autofill::FieldGlobalId focused_field_id) {
  if ((!form_classification.username_field ||
       *form_classification.username_field != focused_field_id) &&
      base::FeatureList::IsEnabled(
          features::kPlusAddressOfferCreationOnAllNonUsernameFields)) {
    return true;
  }
  switch (form_classification.type) {
    case PasswordFormClassification::Type::kNoPasswordForm:
    case PasswordFormClassification::Type::kSignupForm:
      return true;
    case PasswordFormClassification::Type::kLoginForm:
    case PasswordFormClassification::Type::kChangePasswordForm:
    case PasswordFormClassification::Type::kResetPasswordForm:
      return false;
    case PasswordFormClassification::Type::kSingleUsernameForm:
      return base::FeatureList::IsEnabled(
          features::kPlusAddressOfferCreationOnSingleUsernameForms);
  }
  NOTREACHED();
}

// Returns a suggestion to fill an existing plus address.
Suggestion CreateFillPlusAddressSuggestion(std::u16string plus_address) {
  Suggestion suggestion = Suggestion(std::move(plus_address),
                                     SuggestionType::kFillExistingPlusAddress);
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  return suggestion;
}

}  // namespace

PlusAddressSuggestionGenerator::PlusAddressSuggestionGenerator(
    const PlusAddressSettingService* setting_service,
    PlusAddressAllocator* allocator,
    url::Origin origin,
    bool is_off_the_record)
    : setting_service_(CHECK_DEREF(setting_service)),
      allocator_(CHECK_DEREF(allocator)),
      origin_(std::move(origin)),
      is_off_the_record_(is_off_the_record) {}

PlusAddressSuggestionGenerator::~PlusAddressSuggestionGenerator() = default;

std::vector<autofill::Suggestion>
PlusAddressSuggestionGenerator::GetSuggestions(
    const autofill::AutofillClient::PasswordFormClassification&
        focused_form_classification,
    const autofill::FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    std::vector<PlusProfile> affiliated_profiles) {
  using enum autofill::AutofillSuggestionTriggerSource;
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field.value());

  if (affiliated_profiles.empty()) {
    // Do not offer creation in incognito mode.
    if (is_off_the_record_) {
      return {};
    }

    // Do not offer creation if the setting is off.
    if (base::FeatureList::IsEnabled(features::kPlusAddressGlobalToggle) &&
        !setting_service_->GetIsPlusAddressesEnabled()) {
      return {};
    }

    // Do not offer creation on non-empty fields and certain form types (e.g.
    // login forms).
    if (trigger_source != kManualFallbackPlusAddresses &&
        (!normalized_field_value.empty() ||
         !ShouldOfferPlusAddressCreationOnForm(focused_form_classification,
                                               focused_field.global_id()))) {
      return {};
    }

    return {CreateNewPlusAddressSuggestion()};
  }

  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_profiles.size());
  for (const PlusProfile& profile : affiliated_profiles) {
    std::u16string plus_address = base::UTF8ToUTF16(*profile.plus_address);
    // Only suggest filling a plus address whose prefix matches the field's
    // value.
    if (trigger_source == kManualFallbackPlusAddresses ||
        plus_address.starts_with(normalized_field_value)) {
      suggestions.push_back(
          CreateFillPlusAddressSuggestion(std::move(plus_address)));
    }
  }
  return suggestions;
}

// static
Suggestion PlusAddressSuggestionGenerator::GetManagePlusAddressSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

autofill::Suggestion
PlusAddressSuggestionGenerator::CreateNewPlusAddressSuggestion() {
  if (IsInlineGenerationEnabled()) {
    return CreateNewPlusAddressInlineSuggestion();
  }

  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kCreateNewPlusAddress);

  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  suggestion.feature_for_new_badge = &features::kPlusAddressesEnabled;
  suggestion.feature_for_iph =
      &feature_engagement::kIPHPlusAddressCreateSuggestionFeature;
#if BUILDFLAG(IS_ANDROID)
  suggestion.iph_description_text =
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_ANDROID);
#endif  // BUILDFLAG(IS_ANDROID)
  return suggestion;
}

bool PlusAddressSuggestionGenerator::IsInlineGenerationEnabled() const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(
          features::kPlusAddressUserOnboardingEnabled) &&
      !setting_service_->GetHasAcceptedNotice()) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kPlusAddressInlineCreation);
#else
  return false;
#endif
}

// TODO(crbug.com/362445807): Add tests for the inline suggestion once we set
// more suggestion properties.
autofill::Suggestion
PlusAddressSuggestionGenerator::CreateNewPlusAddressInlineSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kCreateNewPlusAddressInline);

  // TODO(crbug.com/362445807): Reconsider the allocation mode.
  if (std::optional<PlusProfile> profile =
          allocator_->AllocatePlusAddressSynchronously(
              origin_, PlusAddressAllocator::AllocationMode::kNewPlusAddress)) {
    suggestion.payload = Suggestion::PlusAddressPayload(
        base::UTF8ToUTF16(profile->plus_address.value()));
  } else {
    suggestion.payload = Suggestion::PlusAddressPayload();
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  // TODO(crbug.com/362445807): Consider adding IPH and new badge for inline
  // suggestions.
  return suggestion;
}

}  // namespace plus_addresses
