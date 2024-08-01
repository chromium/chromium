// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan::internal {

bool LightweightDetectorState::IsValid() const {
  if (!metadata_addr) {
    return false;
  }

  if (num_metadata > kMaxMetadata) {
    return false;
  }

  return true;
}

LightweightDetectorState::SlotMetadata::SlotMetadata() = default;

LightweightDetectorState::SlotMetadata&
LightweightDetectorState::GetSlotMetadataById(
    MetadataId id,
    LightweightDetectorState::SlotMetadata* metadata_arr) {
  return metadata_arr[id % num_metadata];
}

bool LightweightDetectorState::HasMetadataForId(
    MetadataId id,
    LightweightDetectorState::SlotMetadata* metadata_arr) {
  return GetSlotMetadataById(id, metadata_arr).id == id;
}

}  // namespace gwp_asan::internal
