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
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config_internal.h"
#include "components/private_verification_tokens/common/private_verification_tokens_parameters.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace private_verification_tokens {

namespace internal {

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

std::optional<std::vector<url::Origin>> GetValidRedeemers(
    const base::DictValue& dict,
    std::string_view issuer_etld_plus_one,
    const PrivateVerificationTokensParameters& params) {
  const base::ListValue* redeemers_list = dict.FindList(kRedeemersKey);
  if (!redeemers_list) {
    return std::nullopt;
  }
  if (redeemers_list->size() >
      static_cast<size_t>(params.max_number_of_redeemers)) {
    return std::nullopt;
  }

  std::vector<url::Origin> redeemers;
  redeemers.reserve(redeemers_list->size());

  for (const base::Value& item : *redeemers_list) {
    if (!item.is_string()) {
      return std::nullopt;
    }
    url::Origin redeemer_origin = url::Origin::Create(GURL(item.GetString()));
    if (redeemer_origin.scheme() != url::kHttpsScheme) {
      return std::nullopt;
    }
    const std::string redeemer_etld_plus_one =
        net::registry_controlled_domains::GetDomainAndRegistry(
            redeemer_origin,
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    if (redeemer_etld_plus_one.empty() ||
        redeemer_etld_plus_one != issuer_etld_plus_one) {
      return std::nullopt;
    }
    redeemers.push_back(std::move(redeemer_origin));
  }

  return redeemers;
}

std::optional<IssuerConfig> ParseEntry(const base::DictValue& dict) {
  const std::string* origin_str = dict.FindString(kOriginKey);
  if (!origin_str) {
    return std::nullopt;
  }

  url::Origin origin = url::Origin::Create(GURL(*origin_str));
  if (origin.scheme() != url::kHttpsScheme) {
    return std::nullopt;
  }

  const std::string issuer_etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if (issuer_etld_plus_one.empty()) {
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

  std::optional<int> batch_size = internal::GetValidBatchSize(dict, *params);
  if (!batch_size) {
    return std::nullopt;
  }

  std::optional<int64_t> expiration = internal::GetValidExpiration(dict);
  if (!expiration) {
    return std::nullopt;
  }

  std::optional<std::vector<url::Origin>> redeemers =
      internal::GetValidRedeemers(dict, issuer_etld_plus_one, *params);
  if (!redeemers) {
    return std::nullopt;
  }

  PrivateVerificationTokensPublicKey pk(
      std::move(origin), std::move(*decoded_public_key),
      base::Time::UnixEpoch() + base::Seconds(*expiration), *version);
  return IssuerConfig(*batch_size, std::move(pk), std::move(*redeemers));
}

}  // namespace internal

IssuerConfig::IssuerConfig(int32_t batch_size,
                           PrivateVerificationTokensPublicKey public_key,
                           std::vector<url::Origin> redeemers)
    : batch_size(batch_size),
      public_key(std::move(public_key)),
      redeemers(std::move(redeemers)) {}

IssuerConfig::IssuerConfig(const IssuerConfig&) = default;
IssuerConfig& IssuerConfig::operator=(const IssuerConfig&) = default;
IssuerConfig::IssuerConfig(IssuerConfig&&) = default;
IssuerConfig& IssuerConfig::operator=(IssuerConfig&&) = default;
IssuerConfig::~IssuerConfig() = default;

// static
scoped_refptr<PrivateVerificationTokensIssuerConfig>
PrivateVerificationTokensIssuerConfig::Create(base::DictValue config) {
  const base::ListValue* issuers = config.FindList(kIssuersKey);
  if (!issuers) {
    return nullptr;
  }
  std::map<url::Origin, IssuerConfig> result;
  for (const auto& entry : *issuers) {
    if (!entry.is_dict()) {
      continue;
    }
    std::optional<IssuerConfig> ic = internal::ParseEntry(entry.GetDict());
    if (!ic.has_value()) {
      continue;
    }
    url::Origin issuer = ic->public_key.issuer();
    result.try_emplace(issuer, std::move(*ic));
  }
  return base::WrapRefCounted(
      new PrivateVerificationTokensIssuerConfig(std::move(result)));
}

PrivateVerificationTokensIssuerConfig::PrivateVerificationTokensIssuerConfig(
    std::map<url::Origin, IssuerConfig> config)
    : config_(std::move(config)) {}

PrivateVerificationTokensIssuerConfig::
    ~PrivateVerificationTokensIssuerConfig() = default;

// static
scoped_refptr<PrivateVerificationTokensIssuerConfig>
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
  base::DictValue* config_v1 = value->GetDict().FindDict(kConfigVersionKey);
  if (!config_v1) {
    return nullptr;
  }
  return Create(std::move(*config_v1));
}

const std::map<url::Origin, IssuerConfig>&
PrivateVerificationTokensIssuerConfig::config() const {
  return config_;
}

}  // namespace private_verification_tokens
