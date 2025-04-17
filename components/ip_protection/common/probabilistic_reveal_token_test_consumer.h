// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_CONSUMER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_CONSUMER_H_

#include <cstddef>
#include <optional>
#include <string>

#include "components/ip_protection/common/ip_protection_data_types.h"

namespace ip_protection {

// Implements the PRT consumer (a domain that receives PRT) behavior.
// Deserializes the PRT received. This class is a test helper.
class ProbabilisticRevealTokenTestConsumer {
 public:
  static std::optional<ProbabilisticRevealTokenTestConsumer> MaybeCreate(
      const std::string& serialized_prt);
  const ProbabilisticRevealToken& Token() const { return token_; }
  const std::string& EpochId() const { return epoch_id_; }

 private:
  ProbabilisticRevealTokenTestConsumer(ProbabilisticRevealToken token,
                                       std::string epoch_id);
  const ProbabilisticRevealToken token_;
  const std::string epoch_id_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_PROBABILISTIC_REVEAL_TOKEN_TEST_CONSUMER_H_
