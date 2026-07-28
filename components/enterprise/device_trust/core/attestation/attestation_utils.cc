// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/core/attestation/attestation_utils.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/device_trust/core/attestation/signals_type.h"

namespace enterprise_connectors {

std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response) {
  base::DictValue signed_data;

  std::string encoded = base::Base64Encode(challenge_response);

  base::DictValue dict;
  dict.Set("challengeResponse", base::Value(encoded));

  return base::WriteJson(dict).value_or("");
}

}  // namespace enterprise_connectors
