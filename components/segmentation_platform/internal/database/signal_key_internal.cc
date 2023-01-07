// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_key_internal.h"

#include <stdint.h>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/check_op.h"
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
  char output[sizeof(SignalKeyInternal)];
  base::BigEndianWriter writer(output, sizeof(output));
  writer.WriteBytes(&input.prefix.kind, sizeof(input.prefix.kind));
  writer.WriteBytes(&input.prefix.padding, sizeof(input.prefix.padding));
  writer.WriteU64(input.prefix.name_hash);
  base::WriteBigEndian(writer.ptr(), input.time_range_end_sec);
  writer.Skip(sizeof(input.time_range_end_sec));
  base::WriteBigEndian(writer.ptr(), input.time_range_start_sec);
  writer.Skip(sizeof(input.time_range_start_sec));
  CHECK_EQ(0UL, writer.remaining());
  return std::string(output, sizeof(output));
}

bool SignalKeyInternalFromBinary(const std::string& input,
                                 SignalKeyInternal* output) {
  if (input.size() != sizeof(SignalKeyInternal)) {
    ClearKey(output);
    return false;
  }
  auto reader = base::BigEndianReader::FromStringPiece(input);
  reader.ReadBytes(&output->prefix.kind, sizeof(output->prefix.kind));
  reader.Skip(sizeof(SignalKeyInternal::Prefix::padding));
  reader.ReadU64(&output->prefix.name_hash);
  base::ReadBigEndian(reader.ptr(), &output->time_range_end_sec);
  reader.Skip(sizeof(SignalKeyInternal::time_range_end_sec));
  base::ReadBigEndian(reader.ptr(), &output->time_range_start_sec);
  reader.Skip(sizeof(SignalKeyInternal::time_range_start_sec));
  CHECK_EQ(0UL, reader.remaining());
  return true;
}

std::string SignalKeyInternalToDebugString(const SignalKeyInternal& input) {
  std::stringstream buffer;
  buffer << input;
  return buffer.str();
}

std::string SignalKeyInternalPrefixToBinary(
    const SignalKeyInternal::Prefix& input) {
  char output[sizeof(SignalKeyInternal::Prefix)];
  base::BigEndianWriter writer(output, sizeof(output));
  writer.WriteBytes(&input.kind, sizeof(input.kind));
  writer.WriteBytes(&input.padding, sizeof(input.padding));
  writer.WriteU64(input.name_hash);
  CHECK_EQ(0UL, writer.remaining());
  std::string output_str = std::string(output, sizeof(output));
  return output_str;
}

bool SignalKeyInternalPrefixFromBinary(const std::string& input,
                                       SignalKeyInternal::Prefix* output) {
  if (input.size() != sizeof(SignalKeyInternal::Prefix)) {
    ClearKeyPrefix(output);
    return false;
  }
  auto reader = base::BigEndianReader::FromStringPiece(input);
  reader.ReadBytes(&output->kind, sizeof(output->kind));
  reader.Skip(sizeof(SignalKeyInternal::Prefix::padding));
  reader.ReadU64(&output->name_hash);
  CHECK_EQ(0UL, reader.remaining());
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
