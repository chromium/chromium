// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include <utility>

#include "base/check_deref.h"
#include "base/json/values_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace plus_addresses {

namespace {

// Returns whether the end of life of the `preallocated_addresses` has been
// reached or the serialized entry does not have valid format.
bool IsOutdatedOrInvalid(const base::Value& preallocated_address) {
  if (!preallocated_address.is_dict()) {
    return true;
  }
  return base::ValueToTime(preallocated_address.GetDict().Find(
                               PlusAddressPreallocator::kEndOfLifeKey))
             .value_or(base::Time()) <= base::Time::Now();
}

// Stores `profile` in a `base::Value` that can be written to prefs.
base::Value SeralizeAndSetEndOfLife(
    PlusAddressHttpClient::PreallocatedPlusAddress address) {
  return base::Value(
      base::Value::Dict()
          .Set(PlusAddressPreallocator::kEndOfLifeKey,
               base::TimeToValue(base::Time::Now() + address.lifetime))
          .Set(PlusAddressPreallocator::kPlusAddressKey,
               std::move(address.plus_address)));
}

}  // namespace

PlusAddressPreallocator::PlusAddressPreallocator(
    PrefService* pref_service,
    PlusAddressSettingService* setting_service,
    PlusAddressHttpClient* http_client)
    : pref_service_(CHECK_DEREF(pref_service)),
      settings_(CHECK_DEREF(setting_service)),
      http_client_(CHECK_DEREF(http_client)) {
  PrunePreallocatedPlusAddresses();

  // If the notice has not been accepted, we do not preemptively pre-allocate.
  if (settings_->GetHasAcceptedNotice()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &PlusAddressPreallocator::MaybeRequestNewPreallocatedPlusAddresses,
            weak_ptr_factory_.GetWeakPtr()),
        kDelayUntilServerRequestAfterStartup);
  }
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

// TODO: crbug.com/324559503 - Once this method is used by
// `AllocatePlusAddress`, test that no concurrent requests are performed.
void PlusAddressPreallocator::MaybeRequestNewPreallocatedPlusAddresses() {
  if (is_server_request_ongoing_) {
    return;
  }

  if (!settings_->GetIsPlusAddressesEnabled()) {
    return;
  }

  if (static_cast<int>(
          pref_service_->GetList(prefs::kPreallocatedAddresses).size()) >=
      features::kPlusAddressPreallocationMinimumSize.Get()) {
    return;
  }

  // TODO: crbug.com/324559503 - Check whether `PlusAddressService::IsEnabled()`
  // is true.
  is_server_request_ongoing_ = true;
  http_client_->PreallocatePlusAddresses(base::BindOnce(
      &PlusAddressPreallocator::OnReceivePreallocatedPlusAddresses,
      weak_ptr_factory_.GetWeakPtr()));
}

void PlusAddressPreallocator::OnReceivePreallocatedPlusAddresses(
    PlusAddressHttpClient::PreallocatePlusAddressesResult result) {
  is_server_request_ongoing_ = false;
  if (!result.has_value()) {
    // TODO: crbug.com/324559503 - Add error handling.
    return;
  }

  ScopedListPrefUpdate update(&pref_service_.get(),
                              prefs::kPreallocatedAddresses);
  for (PlusAddressHttpClient::PreallocatedPlusAddress& address :
       result.value()) {
    update->Append(SeralizeAndSetEndOfLife(std::move(address)));
  }

  // TODO: crbug.com/324559503 - Check whether there are any pending allocation
  // requests and handle them.
}

}  // namespace plus_addresses
