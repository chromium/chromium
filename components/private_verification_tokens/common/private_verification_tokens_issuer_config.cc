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
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace private_verification_tokens {

namespace internal {

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

std::optional<IssuerConfig> ParseEntry(const base::DictValue& dict) {
  const std::string* domain = dict.FindString(kDomainKey);
  if (!domain || !internal::IsRegistrableDomain(*domain)) {
    return std::nullopt;
  }

  std::optional<int> version = internal::GetValidVersion(dict);
  if (!version) {
    return std::nullopt;
  }

  std::optional<PrivateVerificationTokensParameters> params =
      GetParametersForVersion(*version);
  if (!params) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> decoded_public_key =
      internal::GetDecodedPublicKey(dict);
  if (!decoded_public_key) {
    return std::nullopt;
  }

  std::optional<uint32_t> key_id = internal::GetValidKeyId(dict);
  if (!key_id) {
    return std::nullopt;
  }

  std::optional<int> batch_size = internal::GetValidBatchSize(dict, *params);
  if (!batch_size) {
    return std::nullopt;
  }

  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  if (!expiration) {
    return std::nullopt;
  }

  PrivateVerificationTokensPublicKey pk(
      *domain, std::move(*decoded_public_key), *key_id,
      base::Time::UnixEpoch() + base::Seconds(*expiration), *version);
  return IssuerConfig(*batch_size, std::move(pk));
}

}  // namespace internal

IssuerConfig::IssuerConfig(int32_t batch_size,
                           PrivateVerificationTokensPublicKey public_key)
    : batch_size(batch_size), public_key(std::move(public_key)) {}

IssuerConfig::IssuerConfig(const IssuerConfig&) = default;
IssuerConfig& IssuerConfig::operator=(const IssuerConfig&) = default;
IssuerConfig::IssuerConfig(IssuerConfig&&) = default;
IssuerConfig& IssuerConfig::operator=(IssuerConfig&&) = default;
IssuerConfig::~IssuerConfig() = default;

// static
std::unique_ptr<PrivateVerificationTokensIssuerConfig>
PrivateVerificationTokensIssuerConfig::Create(base::DictValue config) {
  const base::ListValue* issuers = config.FindList(kIssuersKey);
  if (!issuers) {
    return nullptr;
  }
  std::map<std::string, IssuerConfig> result;
  for (const auto& entry : *issuers) {
    if (!entry.is_dict()) {
      continue;
    }
    std::optional<IssuerConfig> ic = internal::ParseEntry(entry.GetDict());
    if (!ic.has_value()) {
      continue;
    }
    std::string domain = ic->public_key.etld_plus_one();
    result.try_emplace(std::move(domain), std::move(*ic));
  }
  return base::WrapUnique(
      new PrivateVerificationTokensIssuerConfig(std::move(result)));
}

PrivateVerificationTokensIssuerConfig::PrivateVerificationTokensIssuerConfig(
    std::map<std::string, IssuerConfig> config)
    : config_(std::move(config)) {}

PrivateVerificationTokensIssuerConfig::
    ~PrivateVerificationTokensIssuerConfig() = default;

// static
std::unique_ptr<PrivateVerificationTokensIssuerConfig>
PrivateVerificationTokensIssuerConfig::LoadFromFile(
    const base::FilePath& path) {
  if (path.empty()) {
    return nullptr;
  }
  std::string content;
  if (!base::ReadFileToString(path, &content)) {
    return nullptr;
  }
  std::optional<base::Value> value = base::JSONReader::Read(content, 0);
  if (!value || !value->is_dict()) {
    return nullptr;
  }
  return Create(std::move(*value).TakeDict());
}

const std::map<std::string, IssuerConfig>&
PrivateVerificationTokensIssuerConfig::config() const {
  return config_;
}

}  // namespace private_verification_tokens
