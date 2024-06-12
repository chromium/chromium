// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/key_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/metrics/structured/lib/histogram_util.h"
#include "components/metrics/structured/lib/key_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"

namespace metrics::structured {
namespace {

std::string HashToHex(const uint64_t hash) {
  return base::HexEncode(&hash, sizeof(uint64_t));
}

int NowInDays() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

}  // namespace

KeyData::KeyData(std::unique_ptr<StorageDelegate> storage_delegate)
    : storage_delegate_(std::move(storage_delegate)) {
  CHECK(storage_delegate_);
}
KeyData::~KeyData() = default;

uint64_t KeyData::Id(const uint64_t project_name_hash,
                     base::TimeDelta key_rotation_period) {
  if (!storage_delegate_->IsReady()) {
    return 0u;
  }

  // Retrieve the key for |project_name_hash|.
  EnsureKeyUpdated(project_name_hash, key_rotation_period);
  const std::optional<std::string_view> key = GetKeyBytes(project_name_hash);
  if (!key) {
    NOTREACHED_IN_MIGRATION();
    return 0u;
  }

  // Compute and return the hash.
  uint64_t hash;
  crypto::SHA256HashString(key.value(), &hash, sizeof(uint64_t));
  return hash;
}

uint64_t KeyData::HmacMetric(const uint64_t project_name_hash,
                             const uint64_t metric_name_hash,
                             const std::string& value,
                             base::TimeDelta key_rotation_period) {
  if (!storage_delegate_->IsReady()) {
    return 0u;
  }

  // Retrieve the key for |project_name_hash|.
  EnsureKeyUpdated(project_name_hash, key_rotation_period);
  const std::optional<std::string_view> key = GetKeyBytes(project_name_hash);
  if (!key) {
    NOTREACHED_IN_MIGRATION();
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

std::optional<base::TimeDelta> KeyData::LastKeyRotation(
    const uint64_t project_name_hash) const {
  const KeyProto* key = storage_delegate_->GetKey(project_name_hash);
  if (!key) {
    return std::nullopt;
  }
  return base::Days(key->last_rotation());
}

std::optional<int> KeyData::GetKeyAgeInWeeks(uint64_t project_name_hash) const {
  std::optional<base::TimeDelta> last_rotation =
      LastKeyRotation(project_name_hash);
  if (!last_rotation.has_value()) {
    return std::nullopt;
  }
  const int now = NowInDays();
  const int days_since_rotation = now - last_rotation->InDays();
  return days_since_rotation / 7;
}

void KeyData::Purge() {
  storage_delegate_->Purge();
}

void KeyData::EnsureKeyUpdated(const uint64_t project_name_hash,
                               base::TimeDelta key_rotation_period) {
  CHECK(storage_delegate_->IsReady());

  const int now = NowInDays();
  const int key_rotation_period_days = key_rotation_period.InDays();
  const KeyProto* key = storage_delegate_->GetKey(project_name_hash);

  // Generate or rotate key.
  if (!key || key->last_rotation() == 0) {
    LogKeyValidation(KeyValidationState::kCreated);
    // If the key does not exist, generate a new one. Set the last rotation to a
    // uniformly selected day between today and |key_rotation_period| days
    // ago, to uniformly distribute users amongst rotation cohorts.
    const int rotation_seed = base::RandInt(0, key_rotation_period_days - 1);
    storage_delegate_->UpsertKey(project_name_hash,
                                 base::Days(now - rotation_seed),
                                 key_rotation_period);
  } else if (now - key->last_rotation() > key_rotation_period_days) {
    LogKeyValidation(KeyValidationState::kRotated);
    // If the key is outdated, generate a new one. Update the last rotation
    // such that the user stays in the same cohort.
    //
    // Note that if the max key rotation period has changed, the new rotation
    // period will be used to calculate whether the key should be rotated or
    // not.
    const int new_last_rotation =
        now - (now - key->last_rotation()) % key_rotation_period_days;
    storage_delegate_->UpsertKey(
        project_name_hash, base::Days(new_last_rotation), key_rotation_period);
  } else {
    LogKeyValidation(KeyValidationState::kValid);
  }
}

const std::optional<std::string_view> KeyData::GetKeyBytes(
    const uint64_t project_name_hash) const {
  // Re-fetch the key after keys are rotated.
  const KeyProto* key = storage_delegate_->GetKey(project_name_hash);
  if (!key) {
    return std::nullopt;
  }

  // Return the key unless it's the wrong size, in which case return nullopt.
  const std::string_view key_string = key->key();
  if (key_string.size() != kKeySize) {
    return std::nullopt;
  }
  return key_string;
}

}  // namespace metrics::structured
