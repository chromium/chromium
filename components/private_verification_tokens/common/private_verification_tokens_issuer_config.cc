// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace private_verification_tokens::internal {

bool IsRegistrableDomain(std::string_view domain) {
  if (domain.empty()) {
    return false;
  }
  const std::string domain_and_registry =
      net::registry_controlled_domains::GetDomainAndRegistry(
          domain, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  return (domain == domain_and_registry);
}

std::optional<int> GetValidVersion(const base::DictValue& dict) {
  std::optional<int> version = dict.FindInt(kVersionKey);
  if (!version.has_value() || version.value() != 1) {
    return std::nullopt;
  }
  return version;
}

std::optional<std::vector<uint8_t>> GetDecodedPublicKey(
    const base::DictValue& dict) {
  const std::string* public_key_base64 = dict.FindString(kPublicKeyKey);
  if (!public_key_base64) {
    return std::nullopt;
  }
  return base::Base64Decode(*public_key_base64);
}

std::optional<uint32_t> GetValidKeyId(const base::DictValue& dict) {
  std::optional<int> maybe_key_id = dict.FindInt(kKeyIdKey);
  if (!maybe_key_id.has_value()) {
    return std::nullopt;
  }
  if (base::IsValueInRangeForNumericType<uint32_t>(maybe_key_id.value())) {
    return static_cast<uint32_t>(maybe_key_id.value());
  }
  return std::nullopt;
}

std::optional<int> GetValidBatchSize(
    const base::DictValue& dict,
    const PrivateVerificationTokensParameters& params) {
  std::optional<int> maybe_batch_size = dict.FindInt(kBatchSizeKey);
  if (!maybe_batch_size.has_value()) {
    return std::nullopt;
  }
  const bool is_batch_size_valid =
      (maybe_batch_size.value() >= params.min_batch_size) &&
      (maybe_batch_size.value() <= params.max_batch_size);
  if (!is_batch_size_valid) {
    return std::nullopt;
  }
  return maybe_batch_size;
}

std::optional<int64_t> GetValidExpiration(const base::DictValue& dict) {
  const std::string* expiration_str = dict.FindString(kExpirationKey);
  if (!expiration_str) {
    return std::nullopt;
  }
  int64_t expiration;
  if (!base::StringToInt64(*expiration_str, &expiration) || expiration < 0) {
    return std::nullopt;
  }
  return expiration;
}

}  // namespace private_verification_tokens::internal
