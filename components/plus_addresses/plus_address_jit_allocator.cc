// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_jit_allocator.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_types.h"
#include "net/http/http_status_code.h"
#include "url/origin.h"

namespace plus_addresses {

namespace {

// The cooldown period applied if a user exhausts their refresh quota. Refresh
// UI will not be offered until the cooldown has expired (or the user has
// restarted the browser).
constexpr base::TimeDelta kRefreshLimitReachedCooldown = base::Days(1);

}  // namespace

PlusAddressJitAllocator::PlusAddressJitAllocator(
    PlusAddressHttpClient* http_client)
    : http_client_(*http_client) {}

PlusAddressJitAllocator::~PlusAddressJitAllocator() = default;

void PlusAddressJitAllocator::AllocatePlusAddress(
    const url::Origin& origin,
    AllocationMode mode,
    PlusAddressRequestCallback callback) {
  switch (mode) {
    case AllocationMode::kAny: {
      http_client_->ReservePlusAddress(origin, /*refresh=*/false,
                                       std::move(callback));
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
      http_client_->ReservePlusAddress(
          origin, /*refresh=*/true,
          base::BindOnce(&PlusAddressJitAllocator::HandleRefreshResponse,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }
  }
  NOTREACHED();
}

std::optional<PlusProfile>
PlusAddressJitAllocator::AllocatePlusAddressSynchronously(
    const url::Origin& origin,
    AllocationMode mode) {
  return std::nullopt;
}

bool PlusAddressJitAllocator::IsRefreshingSupported(
    const url::Origin& origin) const {
  if (!time_refresh_limit_reached_.is_null() &&
      base::TimeTicks::Now() - time_refresh_limit_reached_ <=
          kRefreshLimitReachedCooldown) {
    return false;
  }
  if (auto it = refresh_attempts_.find(origin);
      it != refresh_attempts_.cend() &&
      it->second >= kMaxPlusAddressRefreshesPerOrigin) {
    return false;
  }
  return true;
}

void PlusAddressJitAllocator::RemoveAllocatedPlusAddress(
    const PlusAddress& plus_address) {
  // This is a no-op for the JIT allocator - if the plus address was created,
  // the backend will ensure that it does not show up again.
}

void PlusAddressJitAllocator::HandleRefreshResponse(
    PlusAddressRequestCallback callback,
    const PlusProfileOrError& profile_or_error) {
  if (!profile_or_error.has_value() &&
      profile_or_error.error() == PlusAddressRequestError::AsNetworkError(
                                      net::HTTP_TOO_MANY_REQUESTS)) {
    // If the server responds with 429, it means that the refresh quota is
    // exhausted.
    time_refresh_limit_reached_ = base::TimeTicks::Now();
  }
  std::move(callback).Run(profile_or_error);
}

}  // namespace plus_addresses
