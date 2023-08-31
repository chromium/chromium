// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/lightweight_detector.h"

#include <random>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/gwp_asan/common/allocation_info.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif

namespace gwp_asan::internal {

LightweightDetector::LightweightDetector(LightweightDetectorMode mode,
                                         size_t num_metadata) {
  CHECK_NE(mode, LightweightDetectorMode::kOff);
  CHECK_LE(num_metadata, LightweightDetectorState::kMaxMetadata);

  state_.mode = mode;
  state_.num_metadata = num_metadata;
  metadata_ =
      std::make_unique<LightweightDetectorState::SlotMetadata[]>(num_metadata);
  state_.metadata_addr = reinterpret_cast<uintptr_t>(metadata_.get());

#if BUILDFLAG(IS_ANDROID)
  // Explicitly allow memory ranges the crash_handler needs to read. This is
  // required for WebView because it has a stricter set of privacy constraints
  // on what it reads from the crashing process.
  for (auto& memory_region : GetInternalMemoryRegions()) {
    crash_reporter::AllowMemoryRange(memory_region.first, memory_region.second);
  }
#endif
}

LightweightDetector::~LightweightDetector() = default;

void LightweightDetector::RecordLightweightDeallocation(void* ptr,
                                                        size_t size) {
  DCHECK(metadata_);
  DCHECK_GT(state_.num_metadata, 0u);

  LightweightDetectorState::MetadataId metadata_offset;
  if (next_metadata_id_.load(std::memory_order_relaxed) < state_.num_metadata) {
    // First, fill up the metadata store. There might be a harmless race between
    // the `load` above and `fetch_add` below.
    metadata_offset = 1;
  } else {
    // Perform random eviction while ensuring `metadata_id` keeps increasing.
    std::uniform_int_distribution<LightweightDetectorState::MetadataId>
        distribution(1, state_.num_metadata);
    base::NonAllocatingRandomBitGenerator generator;
    metadata_offset = distribution(generator);
  }

  auto metadata_id =
      next_metadata_id_.fetch_add(metadata_offset, std::memory_order_relaxed);
  auto& slot_metadata =
      state_.GetSlotMetadataById(metadata_id, metadata_.get());

  slot_metadata.id = metadata_id;
  slot_metadata.alloc_size = size;
  slot_metadata.alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  void* trace[LightweightDetectorState::kMaxStackFrames];
  size_t len = AllocationInfo::GetStackTrace(
      trace, LightweightDetectorState::kMaxStackFrames);
  slot_metadata.dealloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           slot_metadata.deallocation_stack_trace,
           sizeof(slot_metadata.deallocation_stack_trace));
  slot_metadata.dealloc.tid = AllocationInfo::GetCurrentTid();
  slot_metadata.dealloc.trace_collected = true;

  LightweightDetectorState::PseudoAddresss encoded_metadata_id =
      LightweightDetectorState::EncodeMetadataId(metadata_id);
  size_t count = size / sizeof(encoded_metadata_id);
  std::fill_n(static_cast<LightweightDetectorState::PseudoAddresss*>(ptr),
              count, encoded_metadata_id);

  size_t remainder_offset = count * sizeof(encoded_metadata_id);
  size_t remainder_size = size - remainder_offset;
  std::fill_n(static_cast<uint8_t*>(ptr) + remainder_offset, remainder_size,
              LightweightDetectorState::kMetadataRemainder);
}

std::string LightweightDetector::GetCrashKey() const {
  return base::StringPrintf("%zx", reinterpret_cast<uintptr_t>(&state_));
}

std::vector<std::pair<void*, size_t>>
LightweightDetector::GetInternalMemoryRegions() {
  std::vector<std::pair<void*, size_t>> regions;
  regions.emplace_back(&state_, sizeof(state_));
  regions.emplace_back(
      metadata_.get(),
      sizeof(LightweightDetectorState::SlotMetadata) * state_.num_metadata);
  return regions;
}

}  // namespace gwp_asan::internal
