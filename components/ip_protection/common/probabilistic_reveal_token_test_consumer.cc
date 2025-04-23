// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/probabilistic_reveal_token_test_consumer.h"

#include <cstddef>
#include <optional>
#include <string>

#include "components/ip_protection/common/ip_protection_data_types.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"

namespace ip_protection {

namespace {
// Size of a PRT when TLS serialized, before base64 encoding.
constexpr size_t kPRTSize = 79;
constexpr size_t kPRTPointSize = 33;
constexpr size_t kEpochIdSize = 8;
}  // namespace

std::optional<ProbabilisticRevealTokenTestConsumer>
ProbabilisticRevealTokenTestConsumer::MaybeCreate(
    const std::string& serialized_prt) {
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(serialized_prt.data()),
           serialized_prt.size());
  if (CBS_len(&cbs) != kPRTSize) {
    return std::nullopt;
  }
  uint8_t version;
  uint16_t u_size;
  uint16_t e_size;
  std::string u(kPRTPointSize, '0');
  std::string e(kPRTPointSize, '0');
  std::string epoch_id(kEpochIdSize, '0');
  if (!CBS_get_u8(&cbs, &version) || !CBS_get_u16(&cbs, &u_size) ||
      u_size != kPRTPointSize ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(u.data()), u_size) ||
      !CBS_get_u16(&cbs, &e_size) || e_size != kPRTPointSize ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(e.data()), e_size) ||
      !CBS_copy_bytes(&cbs, reinterpret_cast<uint8_t*>(epoch_id.data()),
                      kEpochIdSize)) {
    return std::nullopt;
  }
  ProbabilisticRevealToken token(version, std::move(u), std::move(e));
  return ProbabilisticRevealTokenTestConsumer(std::move(token),
                                              std::move(epoch_id));
}

ProbabilisticRevealTokenTestConsumer::ProbabilisticRevealTokenTestConsumer(
    ProbabilisticRevealToken token,
    std::string epoch_id)
    : token_(std::move(token)), epoch_id_(std::move(epoch_id)) {}

}  // namespace ip_protection
