// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/plus_address_suggestion_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/transliterator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/plus_address_allocator.h"
#include "components/plus_addresses/core/browser/plus_address_service.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/browser/settings/plus_address_setting_service.h"
#include "components/plus_addresses/core/common/features.h"
#include "net/http/http_status_code.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {

namespace {

using autofill::FormFieldData;
using autofill::PasswordFormClassification;
using autofill::Suggestion;
using autofill::SuggestionType;

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
    bool has_accepted_notice) {
  // On Android, there are no labels since the Keyboard Accessory only allows
  // for single line chips.
  if constexpr (BUILDFLAG(IS_ANDROID)) {
    return {};
  }
  if (!has_accepted_notice) {
    return {};
  }

  // On iOS the `forwarding_address` is not shown due to size constraints.
  if constexpr (BUILDFLAG(IS_IOS)) {
    return {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  }

  return {{Suggestion::Text(l10n_util::GetStringUTF16(
      IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
}

}  // namespace

PlusAddressSuggestionHelper::PlusAddressSuggestionHelper(
    const PlusAddressSettingService* setting_service,
    PlusAddressAllocator* allocator,
    url::Origin origin)
    : setting_service_(CHECK_DEREF(setting_service)),
      allocator_(CHECK_DEREF(allocator)),
      origin_(std::move(origin)) {}

PlusAddressSuggestionHelper::~PlusAddressSuggestionHelper() = default;

std::vector<autofill::Suggestion> PlusAddressSuggestionHelper::GetSuggestions(
    const std::vector<std::string>& affiliated_plus_addresses,
    const autofill::FormFieldData& focused_field,
    bool is_plus_address_manually_triggered) {
  const std::u16string normalized_field_value =
      autofill::RemoveDiacriticsAndConvertToLowerCase(focused_field.value());

  std::vector<Suggestion> suggestions;
  suggestions.reserve(affiliated_plus_addresses.size());
  for (const std::string& affiliated_plus_address : affiliated_plus_addresses) {
    std::u16string plus_address = base::UTF8ToUTF16(affiliated_plus_address);
    // Generally, plus address suggestions are only available on fields whose
    // content matches the suggestion text. In cases where the field was
    // previously autofilled or suggestions were manually triggered, no prefix
    // matching should be applied.
    if (is_plus_address_manually_triggered || focused_field.is_autofilled() ||
        plus_address.starts_with(normalized_field_value)) {
      suggestions.push_back(
          CreateFillPlusAddressSuggestion(std::move(plus_address)));
    }
  }
  return suggestions;
}

void PlusAddressSuggestionHelper::RefreshPlusAddressForSuggestion(
    Suggestion& suggestion) {
  CHECK(IsInlineGenerationEnabled());
  suggestion =
      CreateNewPlusAddressInlineSuggestion(/*refreshed_suggestion=*/true);
}

// static
Suggestion PlusAddressSuggestionHelper::GetManagePlusAddressSuggestion() {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
      SuggestionType::kManagePlusAddress);
  suggestion.icon = Suggestion::Icon::kGoogleMonochrome;
  return suggestion;
}

// static
Suggestion PlusAddressSuggestionHelper::GetPlusAddressErrorSuggestion(
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
void PlusAddressSuggestionHelper::SetSuggestedPlusAddressForSuggestion(
    const PlusAddress& plus_address,
    autofill::Suggestion& suggestion) {
  suggestion.payload =
      Suggestion::PlusAddressPayload(base::UTF8ToUTF16(*plus_address));
  SetLoadingStateForSuggestion(/*is_loading=*/false, suggestion);
}

// static
void PlusAddressSuggestionHelper::SetLoadingStateForSuggestion(
    bool is_loading,
    autofill::Suggestion& suggestion) {
  suggestion.is_loading = Suggestion::IsLoading(is_loading);
  suggestion.acceptability = is_loading
                                 ? Suggestion::Acceptability::kUnacceptable
                                 : Suggestion::Acceptability::kAcceptable;
  auto existing_payload =
      suggestion.GetPayload<Suggestion::PlusAddressPayload>();
  existing_payload.offer_refresh = !is_loading;
  suggestion.payload = std::move(existing_payload);
}

autofill::Suggestion
PlusAddressSuggestionHelper::CreateNewPlusAddressSuggestion() {
  if (IsInlineGenerationEnabled()) {
    return CreateNewPlusAddressInlineSuggestion(/*refreshed_suggestion=*/false);
  }

  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kCreateNewPlusAddress);

  suggestion.labels =
      CreateLabelsForCreateSuggestion(setting_service_->GetHasAcceptedNotice());
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  suggestion.feature_for_new_badge = &features::kPlusAddressesEnabled;
  suggestion.iph_metadata = Suggestion::IPHMetadata(
      &feature_engagement::kIPHPlusAddressCreateSuggestionFeature);
  return suggestion;
}

bool PlusAddressSuggestionHelper::IsInlineGenerationEnabled() const {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (!setting_service_->GetHasAcceptedNotice()) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

autofill::Suggestion
PlusAddressSuggestionHelper::CreateNewPlusAddressInlineSuggestion(
    bool refreshed_suggestion) {
  Suggestion suggestion(
      l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
      SuggestionType::kCreateNewPlusAddressInline);

  PlusAddressAllocator::AllocationMode mode =
      refreshed_suggestion
          ? PlusAddressAllocator::AllocationMode::kNewPlusAddress
          : PlusAddressAllocator::AllocationMode::kAny;
  if (std::optional<PlusProfile> profile =
          allocator_->AllocatePlusAddressSynchronously(origin_, mode)) {
    SetSuggestedPlusAddressForSuggestion(profile->plus_address, suggestion);
    // Set IPH and new badge information only if allocation is synchronous.
    // Otherwise, they will be showing only during the loading stage and then be
    // hidden automatically.
    suggestion.feature_for_new_badge = &features::kPlusAddressesEnabled;
    suggestion.iph_metadata = Suggestion::IPHMetadata(
        &feature_engagement::kIPHPlusAddressCreateSuggestionFeature);
    suggestion.voice_over = l10n_util::GetStringFUTF16(
        IDS_PLUS_ADDRESS_CREATE_INLINE_SUGGESTION_A11Y_VOICE_OVER,
        base::UTF8ToUTF16(*profile->plus_address));
  } else {
    suggestion.payload = Suggestion::PlusAddressPayload();
    SetLoadingStateForSuggestion(/*is_loading=*/true, suggestion);
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  suggestion.labels =
      CreateLabelsForCreateSuggestion(setting_service_->GetHasAcceptedNotice());
  return suggestion;
}

}  // namespace plus_addresses
