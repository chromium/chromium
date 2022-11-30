// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_key.h"

#include <ostream>
#include <sstream>
#include <string>

#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key_internal.h"

namespace segmentation_platform {

namespace {
char ToInternalSignalKindRepresentation(SignalKey::Kind kind) {
  switch (kind) {
    case SignalKey::Kind::USER_ACTION:
      return 'u';
    case SignalKey::Kind::HISTOGRAM_VALUE:
      return 'h';
    case SignalKey::Kind::HISTOGRAM_ENUM:
      return 'e';
    default:
      return '0';
  }
}

SignalKey::Kind FromInternalSignalKindRepresentation(char kind) {
  switch (kind) {
    case 'u':
      return SignalKey::Kind::USER_ACTION;
    case 'h':
      return SignalKey::Kind::HISTOGRAM_VALUE;
    case 'e':
      return SignalKey::Kind::HISTOGRAM_ENUM;
    default:
      return SignalKey::Kind::UNKNOWN;
  }
}

base::Time StripResolutionSmallerThanSeconds(base::Time time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(time.ToDeltaSinceWindowsEpoch().InSeconds()));
}
}  // namespace

SignalKey::SignalKey(Kind kind,
                     uint64_t name_hash,
                     base::Time range_start,
                     base::Time range_end)
    : kind_(kind),
      name_hash_(name_hash),
      range_start_(StripResolutionSmallerThanSeconds(range_start)),
      range_end_(StripResolutionSmallerThanSeconds(range_end)) {}

SignalKey::SignalKey() : kind_(Kind::UNKNOWN), name_hash_(0) {}

SignalKey::~SignalKey() = default;

bool SignalKey::IsValid() const {
  return kind_ != Kind::UNKNOWN && name_hash_ != 0 && !range_start_.is_null() &&
         !range_end_.is_null();
}

std::string SignalKey::ToBinary() const {
  SignalKeyInternal internal_key;
  internal_key.prefix.kind = ToInternalSignalKindRepresentation(kind_);
  internal_key.prefix.name_hash = name_hash_;
  internal_key.time_range_end_sec =
      range_end_.ToDeltaSinceWindowsEpoch().InSeconds();
  internal_key.time_range_start_sec =
      range_start_.ToDeltaSinceWindowsEpoch().InSeconds();
  return SignalKeyInternalToBinary(internal_key);
}

std::string SignalKey::GetPrefixInBinary() const {
  SignalKeyInternal::Prefix prefix;
  prefix.kind = ToInternalSignalKindRepresentation(kind_);
  prefix.name_hash = name_hash_;
  return SignalKeyInternalPrefixToBinary(prefix);
}

// static
bool SignalKey::FromBinary(const std::string& input, SignalKey* output) {
  SignalKeyInternal internal_key;
  if (!SignalKeyInternalFromBinary(input, &internal_key))
    return false;
  output->kind_ =
      FromInternalSignalKindRepresentation(internal_key.prefix.kind);
  output->name_hash_ = internal_key.prefix.name_hash;
  output->range_start_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(internal_key.time_range_start_sec));
  output->range_end_ = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(internal_key.time_range_end_sec));
  return true;
}

std::string SignalKey::ToDebugString() const {
  std::stringstream buffer;
  buffer << *this;
  return buffer.str();
}

bool SignalKey::operator<(const SignalKey& other) const {
  if (kind_ != other.kind_)
    return kind_ < other.kind_;
  if (name_hash_ != other.name_hash_)
    return name_hash_ < other.name_hash_;
  if (range_end_ < other.range_end_)
    return range_end_ < other.range_end_;
  return range_start_ < other.range_start_;
}

std::ostream& operator<<(std::ostream& os, const SignalKey& key) {
  return os << "{kind=" << key.kind() << ", name_hash=" << key.name_hash()
            << ", range_start=" << key.range_start()
            << ", range_end=" << key.range_end() << "}";
}

}  // namespace segmentation_platform
