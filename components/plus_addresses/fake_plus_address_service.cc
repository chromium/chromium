// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/fake_plus_address_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
using autofill::Suggestion;
using autofill::SuggestionType;
}

FakePlusAddressService::FakePlusAddressService() = default;

FakePlusAddressService::~FakePlusAddressService() = default;

void FakePlusAddressService::AddObserver(PlusAddressService::Observer* o) {
  NOTIMPLEMENTED();
}

void FakePlusAddressService::RemoveObserver(PlusAddressService::Observer* o) {
  NOTIMPLEMENTED();
}

std::vector<autofill::Suggestion>
FakePlusAddressService::GetSuggestionsFromPlusAddresses(
    const std::vector<std::string>& plus_addresses,
    const url::Origin& last_committed_primary_main_frame_origin,
    bool is_off_the_record,
    const autofill::PasswordFormClassification& focused_form_classification,
    const autofill::FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source) {
  if (IsPlusAddressCreationEnabled(last_committed_primary_main_frame_origin,
                                   is_off_the_record)) {
    Suggestion suggestion(
        l10n_util::GetStringUTF16(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT),
        SuggestionType::kCreateNewPlusAddress);
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
    suggestion.icon = Suggestion::Icon::kPlusAddress;
    suggestion.feature_for_iph =
        &feature_engagement::kIPHPlusAddressCreateSuggestionFeature;
    return {suggestion};
  }

  if (IsPlusAddressFillingEnabled(last_committed_primary_main_frame_origin)) {
    Suggestion suggestion =
        Suggestion(plus_addresses::test::kFakePlusAddressU16,
                   SuggestionType::kFillExistingPlusAddress);
    if constexpr (!BUILDFLAG(IS_ANDROID)) {
      suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
    }
    suggestion.icon = Suggestion::Icon::kPlusAddress;
    return {suggestion};
  }
  return {};
}

autofill::Suggestion FakePlusAddressService::GetManagePlusAddressSuggestion()
    const {
  return Suggestion();
}

void FakePlusAddressService::RecordAutofillSuggestionEvent(
    SuggestionEvent suggestion_event) {
  NOTIMPLEMENTED();
}

void FakePlusAddressService::OnPlusAddressSuggestionShown(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form,
    autofill::FieldGlobalId field,
    SuggestionContext suggestion_context,
    autofill::PasswordFormClassification::Type form_type,
    autofill::SuggestionType suggestion_type) {
  NOTIMPLEMENTED();
}

void FakePlusAddressService::OnClickedRefreshInlineSuggestion(
    const url::Origin& last_committed_primary_main_frame_origin,
    base::span<const autofill::Suggestion> current_suggestions,
    size_t current_suggestion_index,
    UpdateSuggestionsCallback update_suggestions_callback) {
  NOTIMPLEMENTED();
}

void FakePlusAddressService::OnShowedInlineSuggestion(
    const url::Origin& primary_main_frame_origin,
    base::span<const autofill::Suggestion> current_suggestions,
    UpdateSuggestionsCallback update_suggestions_callback) {
  NOTIMPLEMENTED();
}

void FakePlusAddressService::OnAcceptedInlineSuggestion(
    const url::Origin& primary_main_frame_origin,
    base::span<const autofill::Suggestion> current_suggestions,
    size_t current_suggestion_index,
    UpdateSuggestionsCallback update_suggestions_callback,
    HideSuggestionsCallback hide_suggestions_callback,
    PlusAddressCallback fill_field_callback,
    ShowAffiliationErrorDialogCallback show_affiliation_error_dialog,
    ShowErrorDialogCallback show_error_dialog,
    base::OnceClosure reshow_suggestions) {
  NOTIMPLEMENTED();
}

bool FakePlusAddressService::IsPlusAddressFillingEnabled(
    const url::Origin& origin) const {
  return is_plus_address_filling_enabled_;
}

bool FakePlusAddressService::IsPlusAddressFullFormFillingEnabled() const {
  return base::FeatureList::IsEnabled(features::kPlusAddressFullFormFill);
}

bool FakePlusAddressService::IsPlusAddressCreationEnabled(
    const url::Origin& origin,
    bool is_off_the_record) const {
  return should_offer_creation_;
}

bool FakePlusAddressService::IsPlusAddress(
    const std::string& potential_plus_address) const {
  return potential_plus_address == plus_addresses::test::kFakePlusAddress;
}

void FakePlusAddressService::GetAffiliatedPlusProfiles(
    const url::Origin& origin,
    GetPlusProfilesCallback callback) {
  if (should_return_no_affiliated_plus_profiles_) {
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(std::vector<PlusProfile>{PlusProfile(
        kFakeProfileId, FacetURI::FromCanonicalSpec(kFacet),
        PlusAddress(plus_addresses::test::kFakePlusAddress), true)});
  }
}

void FakePlusAddressService::GetAffiliatedPlusAddresses(
    const url::Origin& origin,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  if (should_return_no_affiliated_plus_profiles_) {
    std::move(callback).Run({});
  } else {
    std::move(callback).Run(
        std::vector<std::string>{plus_addresses::test::kFakePlusAddress});
  }
}

void FakePlusAddressService::ReservePlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (should_fail_to_reserve_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
    return;
  }

  if (should_return_quota_error_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError::AsNetworkError(
            net::HTTP_TOO_MANY_REQUESTS)));
    return;
  }

  if (should_return_timeout_error_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError::AsNetworkError(
            net::HTTP_REQUEST_TIMEOUT)));
    return;
  }

  std::move(on_completed)
      .Run(PlusProfile(kFakeProfileId, FacetURI::FromCanonicalSpec(kFacet),
                       PlusAddress(plus_addresses::test::kFakePlusAddress),
                       is_confirmed_));
}

void FakePlusAddressService::ConfirmPlusAddress(
    const url::Origin& origin,
    const PlusAddress& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (should_fail_to_confirm_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
    return;
  }

  if (should_return_quota_error_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError::AsNetworkError(
            net::HTTP_TOO_MANY_REQUESTS)));
    return;
  }

  if (should_return_timeout_error_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError::AsNetworkError(
            net::HTTP_REQUEST_TIMEOUT)));
    return;
  }

  if (should_return_affiliated_plus_profile_on_confirm_) {
    std::move(on_completed)
        .Run(PlusProfile(
            kFakeProfileId,
            FacetURI::FromCanonicalSpec(plus_addresses::test::kAffiliatedFacet),
            PlusAddress(plus_addresses::test::kFakeAffiliatedPlusAddress),
            true));
    return;
  }

  is_confirmed_ = true;
  PlusProfile profile(kFakeProfileId, FacetURI::FromCanonicalSpec(kFacet),
                      std::move(plus_address), is_confirmed_);
  if (on_confirmed_) {
    std::move(on_confirmed_).Run(profile);
    on_confirmed_.Reset();
  }
  std::move(on_completed).Run(profile);
}

void FakePlusAddressService::RefreshPlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (should_fail_to_refresh_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
    return;
  }
  std::move(on_completed)
      .Run(PlusProfile(
          kFakeProfileId, FacetURI::FromCanonicalSpec(kFacet),
          PlusAddress(plus_addresses::test::kFakePlusAddressRefresh),
          is_confirmed_));
}

bool FakePlusAddressService::IsRefreshingSupported(const url::Origin& origin) {
  return true;
}

std::optional<std::string> FakePlusAddressService::GetPrimaryEmail() {
  // Ensure the value is present without requiring identity setup.
  return "plus+primary@plus.plus";
}

base::span<const PlusProfile> FakePlusAddressService::GetPlusProfiles() const {
  return plus_profiles_;
}

std::optional<PlusAddress> FakePlusAddressService::GetPlusAddress(
    const affiliations::FacetURI& facet) const {
  return PlusAddress(plus_addresses::test::kFakePlusAddress);
}

std::optional<PlusProfile> FakePlusAddressService::GetPlusProfile(
    const affiliations::FacetURI& facet) const {
  return plus_profiles_[0];
}

bool FakePlusAddressService::ShouldShowManualFallback(
    const url::Origin& origin,
    bool is_off_the_record) const {
  return true;
}

void FakePlusAddressService::SavePlusProfile(const PlusProfile& profile) {
  NOTREACHED();
}

bool FakePlusAddressService::IsEnabled() const {
  return true;
}

void FakePlusAddressService::ClearState() {
  is_confirmed_ = false;
  should_fail_to_confirm_ = false;
  should_fail_to_reserve_ = false;
  should_fail_to_refresh_ = false;
  is_plus_address_filling_enabled_ = false;
  should_offer_creation_ = false;
  should_return_no_affiliated_plus_profiles_ = false;
  should_return_affiliated_plus_profile_on_confirm_ = false;
  should_return_quota_error_ = false;
  should_return_timeout_error_ = false;
}

}  // namespace plus_addresses
