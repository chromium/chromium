// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/features.h"
#include "url/gurl.h"

namespace plus_addresses {

namespace {
absl::optional<GURL> ValidateAndGetUrl() {
  GURL maybe_url = GURL(kEnterprisePlusAddressServerUrl.Get());
  return maybe_url.is_valid() ? absl::make_optional(maybe_url) : absl::nullopt;
}
}  // namespace

PlusAddressClient::PlusAddressClient() : server_url_(ValidateAndGetUrl()) {}

PlusAddressClient::~PlusAddressClient() = default;

}  // namespace plus_addresses
