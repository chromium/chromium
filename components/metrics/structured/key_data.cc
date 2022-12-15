// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/key_data.h"

#include <memory>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "components/metrics/structured/histogram_util.h"
#include "crypto/hmac.h"
#include "crypto/sha2.h"

namespace metrics::structured {
namespace {

// The expected size of a key, in bytes.
constexpr size_t kKeySize = 32;

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

}  // namespace

KeyData::KeyData(const base::FilePath& path,
                 const base::TimeDelta& save_delay,
                 base::OnceCallback<void()> on_initialized)
    : on_initialized_(std::move(on_initialized)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proto_ = std::make_unique<PersistentProto<KeyDataProto>>(
      path, save_delay,
      base::BindOnce(&KeyData::OnRead, weak_factory_.GetWeakPtr()),
      base::BindRepeating(&KeyData::OnWrite, weak_factory_.GetWeakPtr()));
}

KeyData::~KeyData() = default;

void KeyData::OnRead(const ReadStatus status) {
  is_initialized_ = true;
  switch (status) {
    case ReadStatus::kOk:
    case ReadStatus::kMissing:
      break;
    case ReadStatus::kReadError:
      LogInternalError(StructuredMetricsError::kKeyReadError);
      break;
    case ReadStatus::kParseError:
      LogInternalError(StructuredMetricsError::kKeyParseError);
      break;
  }

  std::move(on_initialized_).Run();
}

void KeyData::OnWrite(const WriteStatus status) {
  switch (status) {
    case WriteStatus::kOk:
      break;
    case WriteStatus::kWriteError:
      LogInternalError(StructuredMetricsError::kKeyWriteError);
      break;
    case WriteStatus::kSerializationError:
      LogInternalError(StructuredMetricsError::kKeySerializationError);
      break;
  }
}

void KeyData::WriteNowForTest() {
  proto_.get()->StartWrite();
}

//---------------
// Key management
//---------------

absl::optional<std::string> KeyData::ValidateAndGetKey(
    const uint64_t project_name_hash,
    int key_rotation_period) {
  if (!is_initialized_) {
    NOTREACHED();
    return absl::nullopt;
  }

  const int now = (base::Time::Now() - base::Time::UnixEpoch()).InDays();
  KeyProto& key = (*(proto_.get()->get()->mutable_keys()))[project_name_hash];

  // Generate or rotate key.
  const int last_rotation = key.last_rotation();

  if (key.key().empty() || last_rotation == 0) {
    LogKeyValidation(KeyValidationState::kCreated);
    // If the key is empty, generate a new one. Set the last rotation to a
    // uniformly selected day between today and |key_rotation_period| days
    // ago, to uniformly distribute users amongst rotation cohorts.
    const int rotation_seed = base::RandInt(0, key_rotation_period - 1);
    UpdateKey(&key, now - rotation_seed, key_rotation_period);
  } else if (now - last_rotation > key_rotation_period) {
    LogKeyValidation(KeyValidationState::kRotated);

    // If the key is outdated, generate a new one. Update the last rotation
    // such that the user stays in the same cohort.
    //
    // Note that if the max key rotation period has changed, the new rotation
    // period will be used to calculate whether the key should be rotated or
    // not.
    const int new_last_rotation =
        now - (now - last_rotation) % key_rotation_period;
    UpdateKey(&key, new_last_rotation, key_rotation_period);
  } else {
    LogKeyValidation(KeyValidationState::kValid);
  }

  // Return the key unless it's the wrong size, in which case return nullopt.
  const std::string key_string = key.key();
  if (key_string.size() != kKeySize) {
    LogInternalError(StructuredMetricsError::kWrongKeyLength);
    return absl::nullopt;
  }
  return key_string;
}

void KeyData::UpdateKey(KeyProto* key,
                        int last_key_rotation,
                        int key_rotation_period) {
  key->set_key(GenerateKey());
  key->set_last_rotation(last_key_rotation);
  key->set_rotation_period(key_rotation_period);
  proto_->QueueWrite();
}

//----------------
// IDs and hashing
//----------------

uint64_t KeyData::Id(const uint64_t project_name_hash,
                     int key_rotation_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Retrieve the key for |project_name_hash|.
  const absl::optional<std::string> key =
      ValidateAndGetKey(project_name_hash, key_rotation_period);
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
                             const std::string& value,
                             int key_rotation_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Retrieve the key for |project_name_hash|.
  const absl::optional<std::string> key =
      ValidateAndGetKey(project_name_hash, key_rotation_period);
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

//-----
// Misc
//-----

absl::optional<int> KeyData::LastKeyRotation(const uint64_t project_name_hash) {
  const auto& keys = proto_.get()->get()->keys();
  const auto& it = keys.find(project_name_hash);
  if (it != keys.end()) {
    return it->second.last_rotation();
  }
  return absl::nullopt;
}

void KeyData::Purge() {
  proto_->Purge();
}

}  // namespace metrics::structured
