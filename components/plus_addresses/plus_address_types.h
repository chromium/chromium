// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_

#include <string>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"

// A common place for PlusAddress types to be defined.
namespace plus_addresses {

struct PlusProfile {
  std::string facet;
  std::string plus_address;
  bool is_confirmed;
};

enum class PlusAddressRequestErrorType {
  kParsingError = 0,
  kNetworkError = 1,
  kOAuthError = 2,
};

// TODO: crbug.com/1467623 - This should have more structured information (e.g.
// HTTP response code).
class PlusAddressRequestError {
 public:
  explicit PlusAddressRequestError(PlusAddressRequestErrorType error_type) {
    error_type_ = error_type;
  }

  PlusAddressRequestErrorType type() const { return error_type_; }

 private:
  PlusAddressRequestErrorType error_type_;
};

// Only used by Autofill.
typedef base::OnceCallback<void(const std::string&)> PlusAddressCallback;
typedef std::unordered_map<std::string, std::string> PlusAddressMap;
typedef base::OnceCallback<void(const PlusAddressMap&)> PlusAddressMapCallback;
// Holds either a PlusProfile or an error that prevented us from getting it.
typedef base::expected<PlusProfile, PlusAddressRequestError> PlusProfileOrError;
typedef base::OnceCallback<void(const PlusProfileOrError&)>
    PlusAddressRequestCallback;

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
