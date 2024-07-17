// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/fake_plus_address_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "components/plus_addresses/mock_plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace plus_addresses {

FakePlusAddressService::FakePlusAddressService(
    signin::IdentityManager* identity_manager,
    PlusAddressSettingService* setting_service)
    : PlusAddressService(
          identity_manager,
          setting_service,
          std::make_unique<testing::NiceMock<MockPlusAddressHttpClient>>(),
          /*webdata_service=*/nullptr,
          /*affiliation_service=*/&mock_affiliation_service_,
          /*feature_enabled_for_profile_check=*/
          base::BindRepeating(&base::FeatureList::IsEnabled)) {}

FakePlusAddressService::~FakePlusAddressService() = default;

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
      .Run(
          PlusProfile(kFakeProfileId, kFacet, kFakePlusAddress, is_confirmed_));
}

void FakePlusAddressService::ConfirmPlusAddress(
    const url::Origin& origin,
    const std::string& plus_address,
    PlusAddressRequestCallback on_completed) {
  if (should_fail_to_confirm_) {
    std::move(on_completed)
        .Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kNetworkError)));
    return;
  }
  is_confirmed_ = true;
  PlusProfile profile(kFakeProfileId, kFacet, plus_address, is_confirmed_);
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
      .Run(
          PlusProfile(kFakeProfileId, kFacet, kFakePlusAddress, is_confirmed_));
}

std::optional<std::string> FakePlusAddressService::GetPrimaryEmail() {
  // Ensure the value is present without requiring identity setup.
  return "plus+primary@plus.plus";
}

}  // namespace plus_addresses
