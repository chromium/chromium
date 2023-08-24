// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

// Responsible for communicating with a remote plus-address server.
class PlusAddressClient {
 public:
  PlusAddressClient();
  ~PlusAddressClient();

  absl::optional<GURL> GetServerUrlForTesting() const { return server_url_; }

 private:
  const absl::optional<GURL> server_url_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_CLIENT_H_
