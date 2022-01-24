// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_

#include <string>

#include "chromeos/services/secure_channel/raw_eid_generator.h"

namespace chromeos {

namespace secure_channel {

// Generates raw ephemeral ID (EID) values that are used by the
// ForegroundEidGenerator and BackgroundEidGenerator classes.
class RawEidGeneratorImpl : public RawEidGenerator {
 public:
  RawEidGeneratorImpl();

  RawEidGeneratorImpl(const RawEidGeneratorImpl&) = delete;
  RawEidGeneratorImpl& operator=(const RawEidGeneratorImpl&) = delete;

  ~RawEidGeneratorImpl() override;

  // RawEidGenerator:
  std::string GenerateEid(const std::string& eid_seed,
                          int64_t start_of_period_timestamp_ms,
                          std::string const* extra_entropy) override;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_
