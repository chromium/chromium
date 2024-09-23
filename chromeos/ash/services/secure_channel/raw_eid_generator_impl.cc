// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/raw_eid_generator_impl.h"

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "crypto/sha2.h"

namespace ash::secure_channel {

const int32_t RawEidGenerator::kNumBytesInEidValue = 2;

RawEidGeneratorImpl::RawEidGeneratorImpl() = default;

RawEidGeneratorImpl::~RawEidGeneratorImpl() = default;

std::string RawEidGeneratorImpl::GenerateEid(
    const std::string& eid_seed,
    int64_t start_of_period_timestamp_ms,
    std::string const* extra_entropy) {
  // The data to hash is the eid seed, followed by the extra entropy (if it
  // exists), followed by the timestamp.
  std::string to_hash = eid_seed;
  if (extra_entropy) {
    to_hash += *extra_entropy;
  }
  to_hash.append(base::as_string_view(
      base::numerics::U64ToBigEndian(start_of_period_timestamp_ms)));

  std::string result = crypto::SHA256HashString(to_hash);
  result.resize(RawEidGenerator::kNumBytesInEidValue);
  return result;
}

}  // namespace ash::secure_channel
