// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/colocation/colocation_proof_builder.h"

#include <array>
#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/strcat.h"
#include "components/browser_actuator/colocation/proto/chrome_colocation_proof.pb.h"
#include "google/protobuf/any.pb.h"

namespace browser_actuator {

std::array<uint8_t, kColocationProofPayloadSize> ColocationProofPayload(
    int64_t seconds) {
  std::array<uint8_t, kColocationProofPayloadSize> payload;
  auto payload_span = base::span(payload);
  payload_span.first<kColocationProofLabelSize>().copy_from(
      base::byte_span_from_cstring(kColocationProofLabel));
  payload_span.subspan<kColocationProofLabelSize, sizeof(int64_t)>().copy_from(
      base::I64ToBigEndian(seconds));
  return payload;
}

std::string SerializeColocationProofAsAny(base::span<const uint8_t> signature,
                                          int64_t seconds) {
  ChromeColocationProof proof;
  proof.set_signature(signature.data(), signature.size());
  proof.set_timestamp_seconds(seconds);

  google::protobuf::Any any_proto;
  any_proto.set_type_url(
      base::StrCat({"type.googleapis.com/", proof.GetTypeName()}));
  any_proto.set_value(proof.SerializeAsString());
  return any_proto.SerializeAsString();
}

}  // namespace browser_actuator
