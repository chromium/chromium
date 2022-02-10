// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/ack_handle.h"

#include <stddef.h>
#include <stdint.h>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace invalidation {

namespace {
// Hopefully enough bytes for uniqueness.
const size_t kBytesInHandle = 16;
}  // namespace

AckHandle AckHandle::CreateUnique() {
  // This isn't a valid UUID, so we don't attempt to format it like one.
  uint8_t random_bytes[kBytesInHandle];
  base::RandBytes(random_bytes, sizeof(random_bytes));
  return AckHandle(base::HexEncode(random_bytes, sizeof(random_bytes)),
                   base::Time::Now());
}

AckHandle AckHandle::InvalidAckHandle() {
  return AckHandle(std::string(), base::Time());
}

bool AckHandle::Equals(const AckHandle& other) const {
  return state_ == other.state_ && timestamp_ == other.timestamp_;
}

std::unique_ptr<base::DictionaryValue> AckHandle::ToValue() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetStringKey("state", state_);
  value->SetStringKey("timestamp",
                      base::NumberToString(timestamp_.ToInternalValue()));
  return value;
}

bool AckHandle::ResetFromValue(const base::DictionaryValue& value) {
  if (!value.GetString("state", &state_))
    return false;
  std::string timestamp_as_string;
  if (!value.GetString("timestamp", &timestamp_as_string))
    return false;
  int64_t timestamp_value;
  if (!base::StringToInt64(timestamp_as_string, &timestamp_value))
    return false;
  timestamp_ = base::Time::FromInternalValue(timestamp_value);
  return true;
}

bool AckHandle::IsValid() const {
  return !state_.empty();
}

AckHandle::AckHandle(const std::string& state, base::Time timestamp)
    : state_(state), timestamp_(timestamp) {
}

AckHandle::AckHandle(const AckHandle& other) = default;

AckHandle& AckHandle::operator=(const AckHandle& other) = default;

AckHandle::~AckHandle() = default;

}  // namespace invalidation
