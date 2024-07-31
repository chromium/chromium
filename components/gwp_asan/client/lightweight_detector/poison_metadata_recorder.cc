// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/client/lightweight_detector/poison_metadata_recorder.h"

#include <algorithm>
#include <random>

#include "base/strings/stringprintf.h"
#include "components/gwp_asan/client/thread_local_random_bit_generator.h"
#include "components/gwp_asan/common/allocation_info.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif

namespace gwp_asan::internal::lud {

PoisonMetadataRecorder::PoisonMetadataRecorder(LightweightDetectorMode mode,
                                               size_t num_metadata) {
  CHECK_NE(mode, LightweightDetectorMode::kOff);
  CHECK_LE(num_metadata, LightweightDetectorState::kMaxMetadata);

  ThreadLocalRandomBitGenerator::InitIfNeeded();

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

PoisonMetadataRecorder::~PoisonMetadataRecorder() = default;

void PoisonMetadataRecorder::RecordAndZap(void* ptr, size_t size) {
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
    ThreadLocalRandomBitGenerator generator;
    metadata_offset = distribution(generator);
  }

  auto metadata_id =
      next_metadata_id_.fetch_add(metadata_offset, std::memory_order_relaxed);
  auto& slot_metadata =
      state_.GetSlotMetadataById(metadata_id, metadata_.get());

  slot_metadata.id = metadata_id;
  slot_metadata.alloc_size = size;
  slot_metadata.alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  const void* trace[LightweightDetectorState::kMaxStackFrames];
  size_t len = AllocationInfo::GetStackTrace(trace);
  slot_metadata.dealloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           slot_metadata.deallocation_stack_trace,
           sizeof(slot_metadata.deallocation_stack_trace));
  slot_metadata.dealloc.tid = AllocationInfo::GetCurrentTid();
  slot_metadata.dealloc.trace_collected = true;

  LightweightDetectorState::PseudoAddress encoded_metadata_id =
      LightweightDetectorState::EncodeMetadataId(metadata_id);
  size_t count = size / sizeof(encoded_metadata_id);
  if (count > 0) {
    // This cast is safe (but only assuming -fno-strict-aliasing) because `ptr`
    // is expected to be the beginning of a heap allocation, and heap
    // allocations are required to be aligned. However, this only applies if the
    // allocation was larger than a `PseudoAddress`, so we must guard this with
    // a length check.
    std::fill_n(static_cast<LightweightDetectorState::PseudoAddress*>(ptr),
                count, encoded_metadata_id);
  }

  size_t remainder_offset = count * sizeof(encoded_metadata_id);
  size_t remainder_size = size - remainder_offset;
  std::fill_n(static_cast<uint8_t*>(ptr) + remainder_offset, remainder_size,
              LightweightDetectorState::kMetadataRemainder);
}

std::string PoisonMetadataRecorder::GetCrashKey() const {
  return base::StringPrintf("%zx", reinterpret_cast<uintptr_t>(&state_));
}

std::vector<std::pair<void*, size_t>>
PoisonMetadataRecorder::GetInternalMemoryRegions() {
  std::vector<std::pair<void*, size_t>> regions;
  regions.emplace_back(&state_, sizeof(state_));
  regions.emplace_back(
      metadata_.get(),
      sizeof(LightweightDetectorState::SlotMetadata) * state_.num_metadata);
  return regions;
}

bool PoisonMetadataRecorder::HasAllocationForTesting(uintptr_t address) {
  return std::any_of(
      metadata_.get(), metadata_.get() + state_.num_metadata,
      [&](const auto& metadata) { return metadata.alloc_ptr == address; });
}

template class EXPORT_TEMPLATE_DEFINE(GWP_ASAN_EXPORT)
    SharedStateHolder<PoisonMetadataRecorder>;

}  // namespace gwp_asan::internal::lud
