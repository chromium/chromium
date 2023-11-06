// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/ack_handle.h"

#include <stddef.h>
#include <stdint.h>

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

namespace invalidation {

namespace {
std::string GetRandomId() {
  // Hopefully enough bytes for uniqueness.
  constexpr size_t kBytesInHandle = 16;

  // This isn't a valid UUID, so we don't attempt to format it like one.
  uint8_t random_bytes[kBytesInHandle];
  base::RandBytes(random_bytes, sizeof(random_bytes));

  return base::HexEncode(random_bytes, sizeof(random_bytes));
}
}  // namespace

AckHandle::AckHandle() : state_(GetRandomId()), timestamp_(base::Time::Now()) {}

AckHandle::AckHandle(const AckHandle& other) = default;

AckHandle& AckHandle::operator=(const AckHandle& other) = default;

AckHandle::~AckHandle() = default;

bool AckHandle::Equals(const AckHandle& other) const {
  return state_ == other.state_ && timestamp_ == other.timestamp_;
}

}  // namespace invalidation
