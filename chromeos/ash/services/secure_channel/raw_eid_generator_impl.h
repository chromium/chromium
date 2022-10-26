// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_

#include <string>

#include "chromeos/ash/services/secure_channel/raw_eid_generator.h"

namespace ash::secure_channel {

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

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_RAW_EID_GENERATOR_IMPL_H_
