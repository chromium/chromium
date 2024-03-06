// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_jit_allocator.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "url/origin.h"

namespace plus_addresses {

PlusAddressJitAllocator::PlusAddressJitAllocator(
    PlusAddressService* service,
    PlusAddressHttpClient* http_client)
    : service_(*service), http_client_(*http_client) {}

PlusAddressJitAllocator::~PlusAddressJitAllocator() = default;

void PlusAddressJitAllocator::AllocatePlusAddress(
    const url::Origin& origin,
    AllocationMode mode,
    PlusAddressRequestCallback callback) {
  switch (mode) {
    case AllocationMode::kAny: {
      http_client_->ReservePlusAddress(
          origin,
          base::BindOnce(
              [](PlusAddressService& service, const url::Origin& origin,
                 PlusAddressRequestCallback callback,
                 const PlusProfileOrError& maybe_profile) {
                if (maybe_profile.has_value() && maybe_profile->is_confirmed) {
                  service.SavePlusAddress(origin, maybe_profile->plus_address);
                }
                // Run callback last in case it's dependent on above changes.
                std::move(callback).Run(maybe_profile);
              },
              // Unretained is safe because the service owns the http client.
              base::Unretained(service_), origin, std::move(callback)));
      return;
    }
    case AllocationMode::kNewPlusAddress: {
      int& attempts_made = refresh_attempts_[origin];
      if (attempts_made >= kMaxPlusAddressRefreshesPerOrigin) {
        std::move(callback).Run(base::unexpected(PlusAddressRequestError(
            PlusAddressRequestErrorType::kMaxRefreshesReached)));
        return;
      }
      ++attempts_made;
      // TODO(b/324557932): Implement.
      std::move(callback).Run(base::unexpected(PlusAddressRequestError(
          PlusAddressRequestErrorType::kRequestNotSupportedError)));
      return;
    }
  }
  NOTREACHED_NORETURN();
}

bool PlusAddressJitAllocator::IsRefreshingSupported(
    const url::Origin& origin) const {
  if (auto it = refresh_attempts_.find(origin);
      it != refresh_attempts_.cend() &&
      it->second >= kMaxPlusAddressRefreshesPerOrigin) {
    return false;
  }
  return base::FeatureList::IsEnabled(features::kPlusAddressRefresh);
}

}  // namespace plus_addresses
