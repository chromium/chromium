// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_

#include <iosfwd>
#include <map>
#include <optional>
#include <ostream>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/strong_alias.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"

// A common place for PlusAddress types to be defined.
namespace plus_addresses {

using PlusAddress = base::StrongAlias<struct PlusAddressTag, std::string>;

// A representation of a pre-allocated plus address as received from the server.
struct PreallocatedPlusAddress final {
  PreallocatedPlusAddress();
  PreallocatedPlusAddress(PlusAddress plus_address, base::TimeDelta lifetime);
  PreallocatedPlusAddress(const PreallocatedPlusAddress&);
  PreallocatedPlusAddress& operator=(const PreallocatedPlusAddress&);
  PreallocatedPlusAddress(PreallocatedPlusAddress&&);
  PreallocatedPlusAddress& operator=(PreallocatedPlusAddress&);
  ~PreallocatedPlusAddress();

  // The plus address.
  PlusAddress plus_address;
  // The remaining lifetime relative to when it was requested.
  base::TimeDelta lifetime;

  friend bool operator==(const PreallocatedPlusAddress&,
                         const PreallocatedPlusAddress&) = default;
};

struct PlusProfile {
  PlusProfile(std::optional<std::string> profile_id,
              affiliations::FacetURI facet,
              PlusAddress plus_address,
              bool is_confirmed);
  PlusProfile(const PlusProfile&);
  PlusProfile(PlusProfile&&);
  PlusProfile& operator=(const PlusProfile&) = default;
  PlusProfile& operator=(PlusProfile&&) = default;
  ~PlusProfile();

  friend bool operator==(const PlusProfile&, const PlusProfile&) = default;

  // A unique id used as a primary key for storing confirmed plus addresses.
  // Pre-allocated plus addresses do not have a `profile_id`.
  std::optional<std::string> profile_id;

  // The domain facet to which the plus address is bound.
  affiliations::FacetURI facet;

  // The plus address.
  PlusAddress plus_address;

  // Whether the plus address' creation has been confirmed by the server.
  bool is_confirmed;
};

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
  // The plus address was requested for an invalid, e.g. opaque, origin.
  kInvalidOrigin = 6
};

class PlusAddressRequestError {
 public:
  constexpr explicit PlusAddressRequestError(
      PlusAddressRequestErrorType error_type)
      : error_type_(error_type) {}

  static constexpr PlusAddressRequestError AsNetworkError(
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

  // Returns whether the error signals that the user has hit a quota limit.
  bool IsQuotaError() const;
  // Returns whether the error corresponds to a network timeout.
  bool IsTimeoutError() const;

 private:
  PlusAddressRequestErrorType error_type_;
  // Only set when error_type_ = PlusAddressRequestErrorType::kNetworkError;
  std::optional<int> http_response_code_;
};

class PlusAddressDataChange {
 public:
  enum class Type { kAdd = 0, kRemove = 1 };

  PlusAddressDataChange(Type type, PlusProfile profile);
  PlusAddressDataChange(const PlusAddressDataChange& other);
  PlusAddressDataChange& operator=(const PlusAddressDataChange& change);
  ~PlusAddressDataChange();

  Type type() const { return type_; }
  const PlusProfile& profile() const { return profile_; }

  bool operator==(const PlusAddressDataChange& other) const = default;

 private:
  Type type_;
  PlusProfile profile_;
};

// Only used by Autofill.
using autofill::PlusAddressCallback;

// Holds either a PlusProfile or an error that prevented us from getting it.
using PlusProfileOrError = base::expected<PlusProfile, PlusAddressRequestError>;
using PlusAddressRequestCallback =
    base::OnceCallback<void(const PlusProfileOrError&)>;

// Defined for use in metrics and to share code for certain network-requests.
enum class PlusAddressNetworkRequestType {
  kGetOrCreate = 0,
  kList = 1,
  kReserve = 2,
  kCreate = 3,
  kPreallocate = 4,
  kMaxValue = kPreallocate,
};

std::ostream& operator<<(std::ostream& os,
                         const PreallocatedPlusAddress& address);
std::ostream& operator<<(std::ostream& os, PlusAddressRequestErrorType type);
std::ostream& operator<<(std::ostream& os,
                         const PlusAddressRequestError& error);
std::ostream& operator<<(std::ostream& os, const PlusProfile& profile);
std::ostream& operator<<(std::ostream& os, const PlusProfileOrError& profile);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_TYPES_H_
