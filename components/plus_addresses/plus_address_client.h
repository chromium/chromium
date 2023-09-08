// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_

#include "base/functional/callback_forward.h"
#include "components/plus_addresses/plus_address_auth_token_provider.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}

namespace signin {
class IdentityManager;
}

namespace plus_addresses {

typedef base::OnceCallback<void(const std::string&)> PlusAddressCallback;

// This endpoint is used for most plus-address operations.
constexpr char kServerPlusProfileEndpoint[] = "v1/profiles";

// Utility class for communicating with a remote plus-address server.
class PlusAddressClient {
 public:
  PlusAddressClient(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PlusAddressClient();

  absl::optional<GURL> GetServerUrlForTesting() const { return server_url_; }

  // Initiates a request to get a plus address for use on `site` and only
  // runs `callback` with a plus address if the request to the server
  // completes successfully and returns the expected response.
  void CreatePlusAddress(const std::string& site, PlusAddressCallback callback);

  // Makes the GetOrCreate network request now that it has the OAuth `token`.
  // TODO(kaklilu): Combine this class with the TokenProvider and consolidate
  // this method with the above.
  void CreatePlusAddressWithToken(const std::string& site,
                                  PlusAddressCallback callback,
                                  const std::string& token);

  // Helper to test the plus_address parsing logic.
  static absl::optional<std::string> ParsePlusAddressFromV1CreateForTesting(
      data_decoder::DataDecoder::ValueOrError response);

 private:
  // Used to make HTTP requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // We only support loading a single url at once.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  PlusAddressAuthTokenProvider auth_token_provider_;
  const absl::optional<GURL> server_url_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
