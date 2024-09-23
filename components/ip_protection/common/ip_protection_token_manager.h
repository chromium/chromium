// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MANAGER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MANAGER_H_

#include <optional>
#include <string>

#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// Manages the cache of blind-signed auth tokens.
//
// This class is responsible for checking, fetching, and refilling auth tokens
// for IpProtectionCore.
class IpProtectionTokenManager {
 public:
  virtual ~IpProtectionTokenManager() = default;

  // Check whether tokens are available for the current geo id.
  //
  // This function is called on every URL load, so it should complete quickly.
  virtual bool IsAuthTokenAvailable() = 0;

  // Check whether tokens are available for a particular geo id.
  //
  // This function is called on every URL load, so it should complete quickly.
  // If a geo_id is not provided and token caching by geo is not enabled, this
  // will return false.
  virtual bool IsAuthTokenAvailable(const std::string& geo_id) = 0;

  // Get a token, if one is available for the current geo.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual std::optional<BlindSignedAuthToken> GetAuthToken() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual std::optional<BlindSignedAuthToken> GetAuthToken(
      const std::string& geo_id) = 0;

  // Invalidate any previous instruction that token requests should not be made
  // until after a specified time.
  virtual void InvalidateTryAgainAfterTime() = 0;

  // Returns the current geo id. If no current geo id has been sent, an empty
  // string will be returned. If token caching by geo is disabled, this will
  // always return "EARTH".
  virtual std::string CurrentGeo() const = 0;

  // Set the "current" geo of the token cache manager. This function should only
  // be called by the `IpProtectionCore` for when a geo change has been
  // observed.
  virtual void SetCurrentGeo(const std::string& geo_id) = 0;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_MANAGER_H_
