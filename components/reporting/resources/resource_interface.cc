// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/resources/resource_interface.h"

#include <utility>

#include <cstdint>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

ScopedReservation::ScopedReservation(
    uint64_t size,
    scoped_refptr<ResourceInterface> resource_interface) noexcept
    : resource_interface_(resource_interface) {
  if (!resource_interface->Reserve(size)) {
    return;
  }
  size_ = size;
}

ScopedReservation::ScopedReservation(ScopedReservation&& other) noexcept
    : resource_interface_(other.resource_interface_),
      size_(std::exchange(other.size_, absl::nullopt)) {}

bool ScopedReservation::reserved() const {
  return size_.has_value();
}

bool ScopedReservation::Reduce(uint64_t new_size) {
  if (!reserved()) {
    return false;
  }
  if (new_size < 0 || size_.value() < new_size) {
    return false;
  }
  resource_interface_->Discard(size_.value() - new_size);
  if (new_size > 0) {
    size_ = new_size;
  } else {
    size_ = absl::nullopt;
  }
  return true;
}

void ScopedReservation::HandOver(ScopedReservation& other) {
  DCHECK_EQ(resource_interface_.get(), other.resource_interface_.get())
      << "Reservations are not related";
  if (!other.reserved()) {
    return;  // Nothing changes.
  }
  const uint64_t old_size = (reserved() ? size_.value() : 0uL);
  size_ = old_size + std::exchange(other.size_, absl::nullopt).value();
}

ScopedReservation::~ScopedReservation() {
  if (reserved()) {
    resource_interface_->Discard(size_.value());
  }
}

}  // namespace reporting
