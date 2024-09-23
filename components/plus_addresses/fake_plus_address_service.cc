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
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
using autofill::Suggestion;
using autofill::SuggestionType;
}

FakePlusAddressService::FakePlusAddressService(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    PlusAddressSettingService* setting_service)
    : PlusAddressService(
          pref_service,
          identity_manager,
          setting_service,
          std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
          /*webdata_service=*/nullptr,
          /*affiliation_service=*/&mock_affiliation_service_,
          /*feature_enabled_for_profile_check=*/
          base::BindRepeating(&base::FeatureList::IsEnabled)) {}

FakePlusAddressService::~FakePlusAddressService() = default;

void FakePlusAddressService::GetSuggestions(
    const url::Origin& last_committed_primary_main_frame_origin,
    bool is_off_the_record,
    const autofill::PasswordFormClassification& focused_form_classification,
    const autofill::FormFieldData& focused_field,
    autofill::AutofillSuggestionTriggerSource trigger_source,
    GetSuggestionsCallback callback) {
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
    std::move(callback).Run({suggestion});
    return;
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
    std::move(callback).Run({suggestion});
  }
}

bool FakePlusAddressService::IsPlusAddressFillingEnabled(
    const url::Origin& origin) const {
  return is_plus_address_filling_enabled_;
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

void FakePlusAddressService::ReservePlusAddress(
    const url::Origin& origin,
    PlusAddressRequestCallback on_completed) {
  if (should_fail_to_reserve_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
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

std::optional<std::string> FakePlusAddressService::GetPrimaryEmail() {
  // Ensure the value is present without requiring identity setup.
  return "plus+primary@plus.plus";
}

base::span<const PlusProfile> FakePlusAddressService::GetPlusProfiles() const {
  return plus_profiles_;
}

}  // namespace plus_addresses
