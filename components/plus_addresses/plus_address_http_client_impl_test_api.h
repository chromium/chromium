// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_TEST_API_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_TEST_API_H_

#include <optional>
#include <utility>

#include "base/memory/raw_ref.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "url/gurl.h"

namespace plus_addresses {

class PlusAddressHttpClientImplTestApi {
 public:
  explicit PlusAddressHttpClientImplTestApi(
      PlusAddressHttpClientImpl* http_client)
      : http_client_(*http_client) {}

  std::optional<GURL> GetServerUrlForTesting() && {
    return http_client_->server_url_;
  }

  using TokenReadyCallback = PlusAddressHttpClientImpl::TokenReadyCallback;
  void GetAuthToken(TokenReadyCallback on_fetched) && {
    http_client_->GetAuthToken(std::move(on_fetched));
  }

 private:
  const raw_ref<PlusAddressHttpClientImpl> http_client_;
};

inline PlusAddressHttpClientImplTestApi test_api(
    PlusAddressHttpClientImpl& http_client) {
  return PlusAddressHttpClientImplTestApi(&http_client);
}

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_HTTP_CLIENT_IMPL_TEST_API_H_
