// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include <utility>

#include "base/check_deref.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

PlusAddressPreallocator::PlusAddressPreallocator(
    PlusAddressHttpClient* http_client)
    : http_client_(CHECK_DEREF(http_client)) {}

PlusAddressPreallocator::~PlusAddressPreallocator() = default;

void PlusAddressPreallocator::AllocatePlusAddress(
    const url::Origin& origin,
    AllocationMode mode,
    PlusAddressRequestCallback callback) {
  std::move(callback).Run(base::unexpected(PlusAddressRequestError(
      PlusAddressRequestErrorType::kRequestNotSupportedError)));
}

bool PlusAddressPreallocator::IsRefreshingSupported(
    const url::Origin& origin) const {
  return true;
}

}  // namespace plus_addresses
