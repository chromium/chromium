// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_H_

#include <string>

namespace ash::secure_channel {

// Generates raw ephemeral ID (EID) values that are used by the
// ForegroundEidGenerator and BackgroundEidGenerator classes.
class RawEidGenerator {
 public:
  virtual ~RawEidGenerator() {}

  // The size of an EID in bytes.
  static const int32_t kNumBytesInEidValue;

  // Generates the EID at |start_of_period_timestamp_ms| using the given
  // |eid_seed|. If |extra_entropy| is not null, then it will be used in the EID
  // calculation.
  virtual std::string GenerateEid(const std::string& eid_seed,
                                  int64_t start_of_period_timestamp_ms,
                                  std::string const* extra_entropy) = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_H_
