// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_MOCK_PLUS_ADDRESS_HTTP_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_MOCK_PLUS_ADDRESS_HTTP_CLIENT_H_

#include "base/functional/callback.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

namespace plus_addresses {

class MockPlusAddressHttpClient : public PlusAddressHttpClient {
 public:
  MockPlusAddressHttpClient();
  ~MockPlusAddressHttpClient() override;

  MOCK_METHOD(void,
              ReservePlusAddress,
              (const url::Origin&, bool, PlusAddressRequestCallback),
              (override));
  MOCK_METHOD(void,
              ConfirmPlusAddress,
              (const url::Origin&,
               const PlusAddress&,
               PlusAddressRequestCallback));
  MOCK_METHOD(void,
              PreallocatePlusAddresses,
              (PreallocatePlusAddressesCallback),
              (override));
  MOCK_METHOD(void, Reset, (), (override));
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_MOCK_PLUS_ADDRESS_HTTP_CLIENT_H_
