// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_preallocator.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
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
  const base::Value::Dict& dict = preallocated_address.GetDict();
  if (!dict.FindString(PlusAddressPreallocator::kPlusAddressKey)) {
    return true;
  }
  return base::ValueToTime(dict.Find(PlusAddressPreallocator::kEndOfLifeKey))
             .value_or(base::Time()) <= base::Time::Now();
}

// Stores `profile` in a `base::Value` that can be written to prefs.
base::Value SeralizeAndSetEndOfLife(PreallocatedPlusAddress address) {
  return base::Value(
      base::Value::Dict()
          .Set(PlusAddressPreallocator::kEndOfLifeKey,
               base::TimeToValue(base::Time::Now() + address.lifetime))
          .Set(PlusAddressPreallocator::kPlusAddressKey,
               std::move(*address.plus_address)));
}

// Returns the plus address from its `base::Value` representation. Assumes that
// `preallocated_address` has a valid format.
const std::string& GetPlusAddress(const base::Value& preallocated_address) {
  return *preallocated_address.GetDict().FindString(
      PlusAddressPreallocator::kPlusAddressKey);
}

}  // namespace

PlusAddressPreallocator::PlusAddressPreallocator(
    PrefService* pref_service,
    PlusAddressSettingService* setting_service,
    PlusAddressHttpClient* http_client,
    IsEnabledCheck is_enabled_check)
    : pref_service_(CHECK_DEREF(pref_service)),
      settings_(CHECK_DEREF(setting_service)),
      http_client_(CHECK_DEREF(http_client)),
      is_enabled_check_(std::move(is_enabled_check)) {
  PrunePreallocatedPlusAddresses();

  // If the notice has not been accepted, we do not preemptively pre-allocate.
  if (settings_->GetHasAcceptedNotice() ||
      !base::FeatureList::IsEnabled(
          features::kPlusAddressUserOnboardingEnabled)) {
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
  auto facet = affiliations::FacetURI::FromPotentiallyInvalidSpec(
      origin.GetURL().spec());
  if (!facet.is_valid()) {
    std::move(callback).Run(base::unexpected(
        PlusAddressRequestError(PlusAddressRequestErrorType::kInvalidOrigin)));
    return;
  }
  requests_.emplace(std::move(callback), std::move(facet));
  ProcessAllocationRequests();
}

bool PlusAddressPreallocator::IsRefreshingSupported(
    const url::Origin& origin) const {
  return true;
}

void PlusAddressPreallocator::RemoveAllocatedPlusAddress(
    std::string_view plus_address) {
  {
    ScopedListPrefUpdate update(&pref_service_.get(),
                                prefs::kPreallocatedAddresses);
    update->EraseIf([&](const base::Value& value) {
      return *value.GetDict().FindString(kPlusAddressKey) == plus_address;
    });
  }
  FixIndexOfNextPreallocatedAddress();
}

void PlusAddressPreallocator::FixIndexOfNextPreallocatedAddress() {
  // If there were deletions, update the index of the next plus address to make
  // sure it is in bounds (if non-zero).
  const size_t remaining_plus_addresses = GetPreallocatedAddresses().size();
  // If the disk value was corrupted to something negative, fix it rather than
  // running into bounds check errors later on.
  const int old_index = std::max(GetIndexOfNextPreallocatedAddress(), 0);
  pref_service_->SetInteger(
      prefs::kPreallocatedAddressesNext,
      remaining_plus_addresses
          ? static_cast<int>(old_index % remaining_plus_addresses)
          : 0);
}

void PlusAddressPreallocator::PrunePreallocatedPlusAddresses() {
  {
    ScopedListPrefUpdate update(&pref_service_.get(),
                                prefs::kPreallocatedAddresses);
    update->EraseIf(&IsOutdatedOrInvalid);
  }
  FixIndexOfNextPreallocatedAddress();
}

void PlusAddressPreallocator::MaybeRequestNewPreallocatedPlusAddresses() {
  if (is_server_request_ongoing_) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kPlusAddressGlobalToggle) &&
      !settings_->GetIsPlusAddressesEnabled()) {
    return;
  }

  if (static_cast<int>(GetPreallocatedAddresses().size()) >=
      features::kPlusAddressPreallocationMinimumSize.Get()) {
    return;
  }

  if (!is_enabled_check_.Run()) {
    return;
  }

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
  for (PreallocatedPlusAddress& address : result.value()) {
    update->Append(SeralizeAndSetEndOfLife(std::move(address)));
  }

  ProcessAllocationRequests();
}

void PlusAddressPreallocator::ProcessAllocationRequests() {
  while (!requests_.empty()) {
    std::optional<PlusAddress> next_address = GetNextPreallocatedPlusAddress();
    if (!next_address) {
      break;
    }
    Request request = std::move(requests_.front());
    requests_.pop();
    std::move(request.callback)
        .Run(PlusProfile(/*profile_id=*/std::nullopt,
                         /*facet=*/std::move(request.facet),
                         /*plus_address=*/std::move(next_address).value(),
                         /*is_confirmed=*/false));
  }
  // We may have dipped below the minimum size of the pre-allocated plus address
  // pool that we want to keep around. If so, request new ones.
  MaybeRequestNewPreallocatedPlusAddresses();
}

std::optional<PlusAddress>
PlusAddressPreallocator::GetNextPreallocatedPlusAddress() {
  PrunePreallocatedPlusAddresses();
  const base::Value::List& preallocated_addresses = GetPreallocatedAddresses();
  const int index = GetIndexOfNextPreallocatedAddress();
  if (index >= static_cast<int>(preallocated_addresses.size())) {
    return std::nullopt;
  }
  // Increment the index and return the address.
  pref_service_->SetInteger(
      prefs::kPreallocatedAddressesNext,
      (index + 1) % static_cast<int>(preallocated_addresses.size()));
  return std::make_optional<PlusAddress>(
      GetPlusAddress(preallocated_addresses[index]));
}

const base::Value::List& PlusAddressPreallocator::GetPreallocatedAddresses()
    const {
  return pref_service_->GetList(prefs::kPreallocatedAddresses);
}

int PlusAddressPreallocator::GetIndexOfNextPreallocatedAddress() const {
  return pref_service_->GetInteger(prefs::kPreallocatedAddressesNext);
}

PlusAddressPreallocator::Request::Request(PlusAddressRequestCallback callback,
                                          affiliations::FacetURI facet)
    : callback(std::move(callback)), facet(std::move(facet)) {}

PlusAddressPreallocator::Request::Request(Request&&) = default;

PlusAddressPreallocator::Request& PlusAddressPreallocator::Request::operator=(
    Request&&) = default;

PlusAddressPreallocator::Request::~Request() = default;

}  // namespace plus_addresses
