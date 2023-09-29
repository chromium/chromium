// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_

#include <string>
#include <unordered_map>

#include "base/functional/callback_forward.h"

// A common place for PlusAddress types to be defined.
namespace plus_addresses {

typedef base::OnceCallback<void(const std::string&)> PlusAddressCallback;
typedef std::unordered_map<std::string, std::string> PlusAddressMap;
typedef base::OnceCallback<void(const PlusAddressMap&)> PlusAddressMapCallback;

// Defined for use in metrics and to share code for certain network-requests.
enum class PlusAddressNetworkRequestType {
  kGetOrCreate = 0,
  kList = 1,
  kReserve = 2,
  kCreate = 3,
  kMaxValue = kCreate,
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
