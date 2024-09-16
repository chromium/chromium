// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/ukm_types.h"

namespace segmentation_platform {

CleanupItem::CleanupItem() = default;
CleanupItem::CleanupItem(uint64_t name_hash,
                         uint64_t event_hash,
                         proto::SignalType signal_type,
                         base::Time timestamp)
    : name_hash(name_hash),
      event_hash(event_hash),
      signal_type(signal_type),
      timestamp(timestamp) {}

CleanupItem::~CleanupItem() = default;

bool CleanupItem::operator==(const CleanupItem& other) const {
  return other.name_hash == name_hash && other.event_hash == event_hash &&
         other.signal_type == signal_type && other.timestamp == timestamp;
}

bool CleanupItem::IsUma() const {
  switch (signal_type) {
    case proto::SignalType::USER_ACTION:
    case proto::SignalType::HISTOGRAM_ENUM:
    case proto::SignalType::HISTOGRAM_VALUE:
      return true;
    case proto::SignalType::UNKNOWN_SIGNAL_TYPE:
    case proto::SignalType::UKM_EVENT:
      return false;
  }
}

}  // namespace segmentation_platform
