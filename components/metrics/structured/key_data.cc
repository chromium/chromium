// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data.h"

#include <memory>

#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/structured_events.h"
#include "components/prefs/json_pref_store.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"

namespace metrics {
namespace structured {
namespace internal {
namespace {

// The expected size of a key, in bytes.
constexpr size_t kKeySize = 32;

// The default maximum number of days before rotating keys.
constexpr size_t kDefaultRotationPeriod = 90;

// Generates a key, which is the string representation of
// base::UnguessableToken, and is of size |kKeySize| bytes.
std::string GenerateKey() {
  const std::string key = base::UnguessableToken::Create().ToString();
  DCHECK_EQ(key.size(), kKeySize);
  return key;
}

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

std::string KeyPath(const uint64_t project) {
  return base::StrCat({"keys.", base::NumberToString(project), ".key"});
}

std::string LastRotationPath(const uint64_t project) {
  return base::StrCat(
      {"keys.", base::NumberToString(project), ".last_rotation"});
}

std::string RotationPeriodPath(const uint64_t project) {
  return base::StrCat(
      {"keys.", base::NumberToString(project), ".rotation_period"});
}

}  // namespace

KeyData::KeyData(JsonPrefStore* key_store) : key_store_(key_store) {
  DCHECK(key_store_);
  ValidateKeys();
}

KeyData::~KeyData() = default;

base::Optional<std::string> KeyData::ValidateAndGetKey(
    const uint64_t project_name_hash) {
  DCHECK(key_store_);
  const int now = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  bool key_is_valid = true;

  // If the key for |project_name_hash| doesn't exist, initialize new key data.
  // Set the last rotation to a uniformly selected day between today and
  // |kDefaultRotationPeriod| days ago, to uniformly distribute users amongst
  // rotation cohorts.
  if (!key_store_->GetValue(KeyPath(project_name_hash), nullptr)) {
    const int rotation_seed = base::RandInt(0, kDefaultRotationPeriod - 1);
    SetRotationPeriod(project_name_hash, kDefaultRotationPeriod);
    SetLastRotation(project_name_hash, now - rotation_seed);
    SetKey(project_name_hash, GenerateKey());

    LogKeyValidation(KeyValidationState::kCreated);
    key_is_valid = false;
  }

  // If the key for |project_name_hash| is outdated, generate a new key and
  // write it to the |keys| pref store along with updated rotation data. Update
  // the last rotation such that the user stays in the same cohort.
  const int rotation_period = GetRotationPeriod(project_name_hash);
  const int last_rotation = GetLastRotation(project_name_hash);
  if (now - last_rotation > rotation_period) {
    const int new_last_rotation = now - (now - last_rotation) % rotation_period;
    SetLastRotation(project_name_hash, new_last_rotation);
    SetKey(project_name_hash, GenerateKey());

    LogKeyValidation(KeyValidationState::kRotated);
    key_is_valid = false;
  }

  if (key_is_valid) {
    LogKeyValidation(KeyValidationState::kValid);
  }

  const base::Value* key_json;
  if (!(key_store_->GetValue(KeyPath(project_name_hash), &key_json) &&
        key_json->is_string())) {
    LogInternalError(StructuredMetricsError::kMissingKey);
    return base::nullopt;
  }

  const std::string key = key_json->GetString();
  if (key.size() != kKeySize) {
    LogInternalError(StructuredMetricsError::kWrongKeyLength);
    return base::nullopt;
  }

  return key;
}

void KeyData::ValidateKeys() {
  for (const uint64_t project_name_hash :
       metrics::structured::events::kProjectNameHashes) {
    ValidateAndGetKey(project_name_hash);
  }
}

void KeyData::SetLastRotation(const uint64_t project, const int last_rotation) {
  return key_store_->SetValue(LastRotationPath(project),
                              std::make_unique<base::Value>(last_rotation),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void KeyData::SetRotationPeriod(const uint64_t project,
                                const int rotation_period) {
  return key_store_->SetValue(RotationPeriodPath(project),
                              std::make_unique<base::Value>(rotation_period),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void KeyData::SetKey(const uint64_t project_name_hash, const std::string& key) {
  return key_store_->SetValue(KeyPath(project_name_hash),
                              std::make_unique<base::Value>(key),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

int KeyData::GetLastRotation(const uint64_t project) {
  const base::Value* value;
  if (!(key_store_->GetValue(LastRotationPath(project), &value) &&
        value->is_int())) {
    LogInternalError(StructuredMetricsError::kMissingLastRotation);
    NOTREACHED();
    return 0u;
  }
  return value->GetInt();
}

int KeyData::GetRotationPeriod(const uint64_t project) {
  const base::Value* value;
  if (!(key_store_->GetValue(RotationPeriodPath(project), &value) &&
        value->is_int())) {
    LogInternalError(StructuredMetricsError::kMissingRotationPeriod);
    NOTREACHED();
    return 0u;
  }
  return value->GetInt();
}

uint64_t KeyData::UserProjectId(const uint64_t project_name_hash) {
  // Retrieve the key for |project_name_hash|.
  const base::Optional<std::string> key = ValidateAndGetKey(project_name_hash);
  if (!key) {
    NOTREACHED();
    return 0u;
  }

  // Compute and return the hash.
  uint64_t hash;
  crypto::SHA256HashString(key.value(), &hash, sizeof(uint64_t));
  return hash;
}

uint64_t KeyData::HmacMetric(const uint64_t project_name_hash,
                             const uint64_t metric_name_hash,
                             const std::string& value) {
  // Retrieve the key for |project_name_hash|.
  const base::Optional<std::string> key = ValidateAndGetKey(project_name_hash);
  if (!key) {
    NOTREACHED();
    return 0u;
  }

  // Initialize the HMAC.
  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  CHECK(hmac.Init(key.value()));

  // Compute and return the digest.
  const std::string salted_value =
      base::StrCat({HashToHex(metric_name_hash), value});
  uint64_t digest;
  CHECK(hmac.Sign(salted_value, reinterpret_cast<uint8_t*>(&digest),
                  sizeof(digest)));
  return digest;
}

}  // namespace internal
}  // namespace structured
}  // namespace metrics
