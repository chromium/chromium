// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_

#include <optional>
#include <string>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"

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

class PlusAddressRequestError {
 public:
  explicit PlusAddressRequestError(PlusAddressRequestErrorType error_type) {
    error_type_ = error_type;
  }

  PlusAddressRequestErrorType type() const { return error_type_; }

  void set_http_response_code(int code) {
    CHECK(error_type_ == PlusAddressRequestErrorType::kNetworkError);
    http_response_code_.emplace(code);
  }

  std::optional<int> http_response_code() const { return http_response_code_; }

 private:
  PlusAddressRequestErrorType error_type_;
  // Only set when error_type_ = PlusAddressRequestErrorType::kNetworkError;
  std::optional<int> http_response_code_;
};

// Only used by Autofill.
using autofill::PlusAddressCallback;
using PlusAddressMap = std::unordered_map<std::string, std::string>;
using PlusAddressMapCallback = base::OnceCallback<void(const PlusAddressMap&)>;
// Holds either a PlusProfile or an error that prevented us from getting it.
using PlusProfileOrError = base::expected<PlusProfile, PlusAddressRequestError>;
using PlusAddressRequestCallback =
    base::OnceCallback<void(const PlusProfileOrError&)>;
typedef base::expected<PlusProfile, PlusAddressRequestError> PlusProfileOrError;
typedef base::OnceCallback<void(const PlusProfileOrError&)>
    PlusAddressRequestCallback;
// Holds either a PlusAddressMap or an error that prevented us from getting it.
typedef base::expected<PlusAddressMap, PlusAddressRequestError>
    PlusAddressMapOrError;
typedef base::OnceCallback<void(const PlusAddressMapOrError&)>
    PlusAddressMapRequestCallback;

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
