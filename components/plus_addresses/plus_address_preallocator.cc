// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include <utility>

#include "base/check_deref.h"
#include "base/json/values_util.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace plus_addresses {

namespace {

constexpr std::string_view kEndOfLifeKey = "eol";

// Returns whether the end of life of the `preallocated_addresses` has been
// reached or the serialized entry does not have valid format.
bool IsOutdatedOrInvalid(const base::Value& preallocated_address) {
  if (!preallocated_address.is_dict()) {
    return true;
  }
  return base::ValueToTime(preallocated_address.GetDict().Find(kEndOfLifeKey))
             .value_or(base::Time()) <= base::Time::Now();
}

}  // namespace

PlusAddressPreallocator::PlusAddressPreallocator(
    PrefService* pref_service,
    PlusAddressHttpClient* http_client)
    : pref_service_(CHECK_DEREF(pref_service)),
      http_client_(CHECK_DEREF(http_client)) {
  PrunePreallocatedPlusAddresses();
}

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

void PlusAddressPreallocator::PrunePreallocatedPlusAddresses() {
  ScopedListPrefUpdate update(&pref_service_.get(),
                              prefs::kPreallocatedAddresses);
  if (!update->EraseIf(&IsOutdatedOrInvalid)) {
    return;
  }

  // If there were deletions, update the index of the next plus address to make
  // sure it is in bounds (if non-zero).
  const size_t remaining_plus_addresses = update->size();
  int old_index = pref_service_->GetInteger(prefs::kPreallocatedAddressesNext);
  pref_service_->SetInteger(
      prefs::kPreallocatedAddressesNext,
      remaining_plus_addresses
          ? static_cast<int>(old_index % remaining_plus_addresses)
          : 0);
}

}  // namespace plus_addresses
