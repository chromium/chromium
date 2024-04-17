// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_key_internal.h"

#include <stdint.h>

#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/logging.h"

namespace segmentation_platform {

namespace {

void ClearKeyPrefix(SignalKeyInternal::Prefix* prefix) {
  DCHECK(prefix);
  prefix->kind = 0;
  prefix->name_hash = 0;
}

void ClearKey(SignalKeyInternal* key) {
  DCHECK(key);
  ClearKeyPrefix(&key->prefix);
  key->time_range_end_sec = 0;
  key->time_range_start_sec = 0;
}

}  // namespace

std::string SignalKeyInternalToBinary(const SignalKeyInternal& input) {
  uint8_t output_buf[sizeof(SignalKeyInternal)];
  auto output = base::span(output_buf);

  auto writer = base::SpanWriter(output);
  writer.WriteU8BigEndian(input.prefix.kind);
  writer.Write(base::as_byte_span(input.prefix.padding));
  writer.WriteU64BigEndian(input.prefix.name_hash);
  // SAFETY: If the value is negative we want to store the bit pattern of the
  // negative value, which static_cast preserves. The reader will be required to
  // convert back to a signed value.
  writer.WriteU64BigEndian(static_cast<uint64_t>(input.time_range_end_sec));
  // SAFETY: If the value is negative we want to store the bit pattern of the
  // negative value, which static_cast preserves. The reader will be required to
  // convert back to a signed value.
  writer.WriteU64BigEndian(static_cast<uint64_t>(input.time_range_start_sec));
  CHECK_EQ(writer.remaining(), 0u);
  return std::string(output.begin(), output.end());
}

bool SignalKeyInternalFromBinary(const std::string& input,
                                 SignalKeyInternal* output) {
  if (input.size() != sizeof(SignalKeyInternal)) {
    ClearKey(output);
    return false;
  }
  auto reader = base::SpanReader(base::as_byte_span(input));
  reader.ReadChar(output->prefix.kind);
  reader.Skip(sizeof(SignalKeyInternal::Prefix::padding));
  reader.ReadU64BigEndian(output->prefix.name_hash);
  reader.ReadI64BigEndian(output->time_range_end_sec);
  reader.ReadI64BigEndian(output->time_range_start_sec);
  CHECK_EQ(reader.remaining(), 0u);
  return true;
}

std::string SignalKeyInternalToDebugString(const SignalKeyInternal& input) {
  std::stringstream buffer;
  buffer << input;
  return buffer.str();
}

std::string SignalKeyInternalPrefixToBinary(
    const SignalKeyInternal::Prefix& input) {
  uint8_t output[sizeof(SignalKeyInternal::Prefix)];
  auto writer = base::SpanWriter(base::span(output));
  writer.WriteU8BigEndian(input.kind);
  writer.Write(base::as_byte_span(input.padding));
  writer.WriteU64BigEndian(input.name_hash);
  CHECK_EQ(writer.remaining(), 0u);
  std::string output_str = std::string(std::begin(output), std::end(output));
  return output_str;
}

bool SignalKeyInternalPrefixFromBinary(const std::string& input,
                                       SignalKeyInternal::Prefix* output) {
  if (input.size() != sizeof(SignalKeyInternal::Prefix)) {
    ClearKeyPrefix(output);
    return false;
  }
  auto reader = base::SpanReader(base::as_byte_span(input));
  reader.ReadChar(output->kind);
  reader.Skip(sizeof(SignalKeyInternal::Prefix::padding));
  reader.ReadU64BigEndian(output->name_hash);
  CHECK_EQ(reader.remaining(), 0u);
  return true;
}

std::string SignalKeyInternalPrefixToDebugString(
    const SignalKeyInternal::Prefix& input) {
  std::stringstream buffer;
  buffer << input;
  return buffer.str();
}

std::ostream& operator<<(std::ostream& os,
                         const SignalKeyInternal::Prefix& prefix) {
  return os << "{" << prefix.kind << ":" << prefix.name_hash << "}";
}

std::ostream& operator<<(std::ostream& os, const SignalKeyInternal& key) {
  return os << "{" << key.prefix << ":" << key.time_range_end_sec << ":"
            << key.time_range_start_sec << "}";
}

}  // namespace segmentation_platform
