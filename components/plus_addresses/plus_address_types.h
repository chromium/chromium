// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_

#include <map>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"

// A common place for PlusAddress types to be defined.
namespace plus_addresses {

namespace internal {
// TODO(b/322147254): With three std::string members, the Chromium style checker
// considers `PlusProfile` a complex struct, requiring a user-defined
// constructor. Using a template avoids triggering this check.
// Add a constructor and rewrite all aggregate initialisations.
template <typename = void>
struct PlusProfile {
  std::string profile_id;
  std::string facet;
  std::string plus_address;
  bool is_confirmed;

  friend bool operator==(const PlusProfile&, const PlusProfile&) = default;
};
}  // namespace internal
using PlusProfile = internal::PlusProfile<>;

struct PlusProfileFacetComparator {
  bool operator()(const PlusProfile& a, const PlusProfile& b) const {
    return a.facet < b.facet;
  }
};

enum class PlusAddressRequestErrorType {
  kParsingError = 0,
  kNetworkError = 1,
  kOAuthError = 2,
  // The type of request is not supported by this version of Chrome - e.g.,
  // refreshing plus addresses prior to them being enabled.
  kRequestNotSupportedError = 3,
  // The refresh request is not allowed because the limit of requests has been
  // meet.
  kMaxRefreshesReached = 4,
  // The request could not be fulfilled because the user signed out and the
  // network request was cancelled.
  kUserSignedOut = 5,
};

class PlusAddressRequestError {
 public:
  constexpr explicit PlusAddressRequestError(
      PlusAddressRequestErrorType error_type)
      : error_type_(error_type) {}

  static PlusAddressRequestError AsNetworkError(
      std::optional<int> response_code) {
    PlusAddressRequestError result(PlusAddressRequestErrorType::kNetworkError);
    result.http_response_code_ = response_code;
    return result;
  }

  bool operator==(const PlusAddressRequestError&) const = default;

  PlusAddressRequestErrorType type() const { return error_type_; }

  void set_http_response_code(int code) {
    CHECK(error_type_ == PlusAddressRequestErrorType::kNetworkError);
    http_response_code_ = code;
  }

  std::optional<int> http_response_code() const { return http_response_code_; }

 private:
  PlusAddressRequestErrorType error_type_;
  // Only set when error_type_ = PlusAddressRequestErrorType::kNetworkError;
  std::optional<int> http_response_code_;
};

// Only used by Autofill.
using autofill::PlusAddressCallback;

using PlusAddressMap = std::map<std::string, std::string>;

// Holds either a PlusProfile or an error that prevented us from getting it.
using PlusProfileOrError = base::expected<PlusProfile, PlusAddressRequestError>;
using PlusAddressRequestCallback =
    base::OnceCallback<void(const PlusProfileOrError&)>;

// Holds either a PlusAddressMap or an error that prevented us from getting it.
using PlusAddressMapOrError =
    base::expected<PlusAddressMap, PlusAddressRequestError>;
using PlusAddressMapRequestCallback =
    base::OnceCallback<void(const PlusAddressMapOrError&)>;

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
