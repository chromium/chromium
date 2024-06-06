// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_SERVICE_H_

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace plus_addresses {

class FakePlusAddressService : public PlusAddressService {
 public:
  explicit FakePlusAddressService(signin::IdentityManager* identity_manager);
  ~FakePlusAddressService() override;

  static constexpr char kFakeProfileId[] = "123";
  static constexpr char kFakePlusAddress[] = "plus+remote@plus.plus";
  static constexpr char kFacet[] = "facet.bar";

  // PlusAddressService:
  void ReservePlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override;
  void ConfirmPlusAddress(const url::Origin& origin,
                          const std::string& plus_address,
                          PlusAddressRequestCallback on_completed) override;
  void RefreshPlusAddress(const url::Origin& origin,
                          PlusAddressRequestCallback on_completed) override;
  std::optional<std::string> GetPrimaryEmail() override;

  // Toggles on/off whether `ReservePlusAddress` returns a confirmed
  // `PlusProfile`.
  void set_is_confirmed(bool confirmed) { is_confirmed_ = confirmed; }

  // Sets the callback that is executed if the service receives a confirmed
  // profile.
  void set_confirm_callback(PlusAddressRequestCallback callback) {
    on_confirmed_ = std::move(callback);
  }

  // Toggles on/off whether an error occurs on `ConfirmPlusAddress`.
  void set_should_fail_to_confirm(bool status) {
    should_fail_to_confirm_ = status;
  }

  // Toggles on/off whether an error occurs on `ReservePlusAddress`.
  void set_should_fail_to_reserve(bool status) {
    should_fail_to_reserve_ = status;
  }

  // Toggles on/off whether an error occurs on `RefreshPlusAddress`.
  void set_should_fail_to_refresh(bool status) {
    should_fail_to_refresh_ = status;
  }

 private:
  PlusAddressRequestCallback on_confirmed_;
  testing::NiceMock<affiliations::MockAffiliationService>
      mock_affiliation_service_;
  bool is_confirmed_ = false;
  bool should_fail_to_confirm_ = false;
  bool should_fail_to_reserve_ = false;
  bool should_fail_to_refresh_ = false;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_SERVICE_H_
