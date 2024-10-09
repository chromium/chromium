// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_suggestion_generator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/borrowed_transliterator.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "net/http/http_status_code.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

namespace {

using autofill::FormFieldData;
using autofill::PasswordFormClassification;
using autofill::Suggestion;
using autofill::SuggestionType;

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

// Returns the labels for a create new plus address suggestion.
// `forwarding_address` is the email that traffic is forwarded to.
std::vector<std::vector<Suggestion::Text>> CreateLabelsForCreateSuggestion(
    bool has_accepted_notice,
    std::string_view forwarding_address) {
  // On Android, there are no labels since the Keyboard Accessory only allows
  // for single line chips.
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return {};
  }
  if (!has_accepted_notice &&
      base::FeatureList::IsEnabled(features::kPlusAddressSuggestionRedesign)) {
    return {};
  }

  // On iOS the `forwarding_address` is not shown due to size constraints.
  if constexpr (BUILDFLAG(IS_IOS)) {
    return {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  }

  std::u16string label_text =
      features::kShowForwardingEmailInSuggestion.Get()
          ? l10n_util::GetStringFUTF16(
                IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT_WITH_FORWARDING_INFO,
                base::UTF8ToUTF16(forwarding_address))
          : l10n_util::GetStringUTF16(
                IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT);
  return {{Suggestion::Text(std::move(label_text))}};
}

}  // namespace

PlusAddressSuggestionGenerator::PlusAddressSuggestionGenerator(
    const PlusAddressSettingService* setting_service,
    PlusAddressAllocator* allocator,
    url::Origin origin,
    std::string primary_email)
    : setting_service_(CHECK_DEREF(setting_service)),
      allocator_(CHECK_DEREF(allocator)),
      origin_(std::move(origin)),
      primary_email_(std::move(primary_email)) {}

PlusAddressSuggestionGenerator::~PlusAddressSuggestionGenerator() = default;

std::vector<autofill::Suggestion>
PlusAddressSuggestionGenerator::GetSuggestions(
    const std::vector<std::string>& affiliated_plus_addresses,
    bool is_creation_enabled,
    const autofill::PasswordFormClassification& focused_form_classification,
    const autofill::FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  using enum autofill::AutofillSuggestionTriggerSource;
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field.value());

  if (affiliated_plus_addresses.empty()) {
    // Do not offer creation if disabled.
    if (!is_creation_enabled) {
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
  suggestions.reserve(affiliated_plus_addresses.size());
  for (const std::string& affiliated_plus_addresse :
       affiliated_plus_addresses) {
    std::u16string plus_address = base::UTF8ToUTF16(affiliated_plus_addresse);
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

void PlusAddressSuggestionGenerator::RefreshPlusAddressForSuggestion(
    Suggestion& suggestion) {
  CHECK(IsInlineGenerationEnabled());
  suggestion = CreateNewPlusAddressInlineSuggestion();
}

// static
Suggestion PlusAddressSuggestionGenerator::GetManagePlusAddressSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

// static
Suggestion PlusAddressSuggestionGenerator::GetPlusAddressErrorSuggestion(
    const PlusAddressRequestError& error) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kPlusAddressError);
  suggestion.icon = Suggestion::Icon::kError;

  // Refreshing does not make sense for a quota error, since those will persist
  // for a significant amount of time.
  Suggestion::PlusAddressPayload payload;
  payload.offer_refresh = !error.IsQuotaError();
  suggestion.payload = std::move(payload);

  // The label depends on the error type.
  std::u16string label_text;
  if (error.IsQuotaError()) {
    label_text =
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_RESERVE_QUOTA_ERROR_TEXT);
  } else if (error.IsTimeoutError()) {
    label_text =
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_RESERVE_TIMEOUT_ERROR_TEXT);
  } else {
    label_text =
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_RESERVE_GENERIC_ERROR_TEXT);
  }
  suggestion.labels = {{Suggestion::Text(std::move(label_text))}};
  return suggestion;
}

// static
void PlusAddressSuggestionGenerator::SetSuggestedPlusAddressForSuggestion(
    const PlusAddress& plus_address,
    autofill::Suggestion& suggestion) {
  suggestion.payload =
      Suggestion::PlusAddressPayload(base::UTF8ToUTF16(*plus_address));
  SetLoadingStateForSuggestion(/*is_loading=*/false, suggestion);
}

// static
void PlusAddressSuggestionGenerator::SetLoadingStateForSuggestion(
    bool is_loading,
    autofill::Suggestion& suggestion) {
  suggestion.is_loading = Suggestion::IsLoading(is_loading);
  suggestion.is_acceptable = !is_loading;
  auto existing_payload =
      suggestion.GetPayload<Suggestion::PlusAddressPayload>();
  existing_payload.offer_refresh = !is_loading;
  suggestion.payload = std::move(existing_payload);
}

autofill::Suggestion
PlusAddressSuggestionGenerator::CreateNewPlusAddressSuggestion() {
  if (IsInlineGenerationEnabled()) {
    return CreateNewPlusAddressInlineSuggestion();
  }

  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kCreateNewPlusAddress);

  suggestion.labels = CreateLabelsForCreateSuggestion(
      !base::FeatureList::IsEnabled(
          features::kPlusAddressUserOnboardingEnabled) ||
          setting_service_->GetHasAcceptedNotice(),
      primary_email_);
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
    SetSuggestedPlusAddressForSuggestion(profile->plus_address, suggestion);
    // Set IPH and new badge information only if allocation is synchronous.
    // Otherwise, they will be showing only during the loading stage and then be
    // hidden automatically.
    suggestion.feature_for_new_badge = &features::kPlusAddressesEnabled;
    suggestion.feature_for_iph =
        &feature_engagement::kIPHPlusAddressCreateSuggestionFeature;
  } else {
    suggestion.payload = Suggestion::PlusAddressPayload();
    SetLoadingStateForSuggestion(/*is_loading=*/true, suggestion);
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  suggestion.labels = CreateLabelsForCreateSuggestion(
      !base::FeatureList::IsEnabled(
          features::kPlusAddressUserOnboardingEnabled) ||
          setting_service_->GetHasAcceptedNotice(),
      primary_email_);
  return suggestion;
}

}  // namespace plus_addresses
