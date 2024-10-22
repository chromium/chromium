// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_types.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>

#include "net/http/http_status_code.h"

namespace plus_addresses {

PreallocatedPlusAddress::PreallocatedPlusAddress(PlusAddress plus_address,
                                                 base::TimeDelta lifetime)
    : plus_address(std::move(plus_address)), lifetime(lifetime) {}

PreallocatedPlusAddress::PreallocatedPlusAddress(
    const PreallocatedPlusAddress&) = default;

PreallocatedPlusAddress& PreallocatedPlusAddress::operator=(
    const PreallocatedPlusAddress&) = default;

PreallocatedPlusAddress::PreallocatedPlusAddress(PreallocatedPlusAddress&&) =
    default;

PreallocatedPlusAddress& PreallocatedPlusAddress::operator=(
    PreallocatedPlusAddress&) = default;

PreallocatedPlusAddress::~PreallocatedPlusAddress() = default;

PlusProfile::PlusProfile(std::optional<std::string> profile_id,
                         affiliations::FacetURI facet,
                         PlusAddress plus_address,
                         bool is_confirmed)
    : profile_id(std::move(profile_id)),
      facet(std::move(facet)),
      plus_address(std::move(plus_address)),
      is_confirmed(is_confirmed) {}
PlusProfile::PlusProfile(const PlusProfile&) = default;
PlusProfile::PlusProfile(PlusProfile&&) = default;
PlusProfile::~PlusProfile() = default;

bool PlusAddressRequestError::IsQuotaError() const {
  return error_type_ == PlusAddressRequestErrorType::kNetworkError &&
         http_response_code_.value_or(net::HTTP_REQUEST_TIMEOUT) ==
             net::HTTP_TOO_MANY_REQUESTS;
}

bool PlusAddressRequestError::IsTimeoutError() const {
  return error_type_ == PlusAddressRequestErrorType::kNetworkError &&
         http_response_code_.value_or(net::HTTP_TOO_MANY_REQUESTS) ==
             net::HTTP_REQUEST_TIMEOUT;
}

PlusAddressDataChange::PlusAddressDataChange(Type type, PlusProfile profile)
    : type_(type), profile_(std::move(profile)) {}
PlusAddressDataChange::PlusAddressDataChange(
    const PlusAddressDataChange& other) = default;
PlusAddressDataChange& PlusAddressDataChange::operator=(
    const PlusAddressDataChange& change) = default;
PlusAddressDataChange::~PlusAddressDataChange() = default;

std::ostream& operator<<(std::ostream& os,
                         const PreallocatedPlusAddress& address) {
  return os << "PreallocatedPlusAddress(plus_address=" << *address.plus_address
            << ",lifetime=" << address.lifetime << ")";
}

std::ostream& operator<<(std::ostream& os, PlusAddressRequestErrorType type) {
  return os << [&]() -> std::string_view {
    switch (type) {
      case PlusAddressRequestErrorType::kParsingError:
        return "ParsingError";
      case PlusAddressRequestErrorType::kNetworkError:
        return "NetworkError";
      case PlusAddressRequestErrorType::kOAuthError:
        return "OAuthError";
      case PlusAddressRequestErrorType::kRequestNotSupportedError:
        return "RequestNotSupportedError";
      case PlusAddressRequestErrorType::kMaxRefreshesReached:
        return "MaxRefreshesReached";
      case PlusAddressRequestErrorType::kUserSignedOut:
        return "UserSignedOut";
      case PlusAddressRequestErrorType::kInvalidOrigin:
        return "InvalidOrigin";
    }
  }();
}

std::ostream& operator<<(std::ostream& os,
                         const PlusAddressRequestError& error) {
  os << "PlusAddressRequestError(" << error.type();
  if (error.http_response_code()) {
    os << ",http_response_code=" << *error.http_response_code();
  }
  return os << ")";
}

std::ostream& operator<<(std::ostream& os, const PlusProfile& profile) {
  os << "PlusProfile(profile_id=" << profile.profile_id.value_or("")
     << ",facet=";
  os << profile.facet;
  return os << ",plus_address=" << *profile.plus_address
            << ",is_confirmed=" << profile.is_confirmed << ")";
}

std::ostream& operator<<(std::ostream& os, const PlusProfileOrError& profile) {
  if (profile.has_value()) {
    return os << profile.value();
  }
  return os << profile.error();
}

}  // namespace plus_addresses
