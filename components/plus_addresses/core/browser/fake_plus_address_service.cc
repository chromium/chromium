// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/core/browser/fake_plus_address_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/strings/to_string.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/browser/mock_plus_address_http_client.h"
#include "components/plus_addresses/core/browser/plus_address_hats_utils.h"
#include "components/plus_addresses/core/browser/plus_address_test_utils.h"
#include "components/plus_addresses/core/browser/plus_address_types.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/plus_addresses/core/common/plus_address_prefs.h"
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
    const std::vector<std::string>& plus_addresses) {
  Suggestion suggestion = Suggestion(plus_addresses::test::kFakePlusAddressU16,
                                     SuggestionType::kFillExistingPlusAddress);
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    suggestion.labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  suggestion.icon = Suggestion::Icon::kPlusAddress;
  return {suggestion};
}

autofill::Suggestion FakePlusAddressService::GetManagePlusAddressSuggestion()
    const {
  return Suggestion(autofill::SuggestionType::kManagePlusAddress);
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

void FakePlusAddressService::DidFillPlusAddress() {
  did_fill_plus_address_suggestion_ = true;
}

size_t FakePlusAddressService::GetPlusAddressesCount() {
  return plus_profiles_.size();
}

std::map<std::string, std::string>
FakePlusAddressService::GetPlusAddressHatsData() const {
  return {{hats::kPlusAddressesCount, base::ToString(GetPlusProfiles().size())},
          {hats::kFirstPlusAddressCreationTime, "-1"},
          {hats::kLastPlusAddressFillingTime, "-1"}};
}

bool FakePlusAddressService::IsPlusAddressFillingEnabled(
    const url::Origin& origin) const {
  return is_plus_address_filling_enabled_;
}

bool FakePlusAddressService::IsPlusAddress(
    const std::string& potential_plus_address) const {
  return potential_plus_address == plus_addresses::test::kFakePlusAddress;
}

bool FakePlusAddressService::IsFieldEligibleForPlusAddress(
    const autofill::AutofillField& field) const {
  auto filling_products = autofill::DenseSet<autofill::FillingProduct>(
      field.Type().GetGroups(), &autofill::GetFillingProductFromFieldTypeGroup);

  if (filling_products.contains(autofill::FillingProduct::kAddress)) {
    return true;
  }

  return (field.server_type() == autofill::FieldType::USERNAME ||
          field.server_type() == autofill::FieldType::SINGLE_USERNAME) &&
         field.heuristic_type() == autofill::FieldType::EMAIL_ADDRESS;
}

bool FakePlusAddressService::MatchesPlusAddressFormat(
    const std::u16string& value) const {
  return value.ends_with(u"@grelay.com");
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
  should_return_no_affiliated_plus_profiles_ = false;
  should_return_affiliated_plus_profile_on_confirm_ = false;
  should_return_quota_error_ = false;
  should_return_timeout_error_ = false;
}

}  // namespace plus_addresses
