// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_POISON_METADATA_RECORDER_H_
#define COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_POISON_METADATA_RECORDER_H_

#include <atomic>
#include <memory>
#include <vector>

#include "base/export_template.h"
#include "base/gtest_prod_util.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/lightweight_detector/shared_state.h"
#include "components/gwp_asan/common/lightweight_detector_state.h"

namespace gwp_asan::internal {
FORWARD_DECLARE_TEST(LightweightDetectorAnalyzerTest, InternalError);
}  // namespace gwp_asan::internal

namespace gwp_asan::internal::lud {

// Responsible for both poisoning memory allocations and tracking metadata
// associated with these poisoned allocations.
class GWP_ASAN_EXPORT PoisonMetadataRecorder
    : public SharedState<PoisonMetadataRecorder> {
 public:
  PoisonMetadataRecorder(const PoisonMetadataRecorder&) = delete;
  PoisonMetadataRecorder& operator=(const PoisonMetadataRecorder&) = delete;

  // Records the deallocation stack trace and overwrites the allocation with a
  // pattern that allows the crash handler to recover the trace ID.
  void RecordAndZap(void* ptr, size_t size);

  // Retrieves the textual address of the shared state required by the
  // crash handler.
  std::string GetCrashKey() const;

  // Returns internal memory used by the detector (required for sanitization
  // on supported platforms.)
  std::vector<std::pair<void*, size_t>> GetInternalMemoryRegions();

  bool HasAllocationForTesting(uintptr_t);

 private:
  // Since the allocator hooks cannot be uninstalled, and they access an
  // instance of this class, it's unsafe to ever destroy it outside unit tests.
  PoisonMetadataRecorder(LightweightDetectorMode, size_t num_metadata);
  ~PoisonMetadataRecorder();

  // The state shared with with the crash analyzer.
  LightweightDetectorState state_;
  // Array of metadata (e.g. stack traces) for allocations.
  std::unique_ptr<LightweightDetectorState::SlotMetadata[]> metadata_;

  std::atomic<LightweightDetectorState::MetadataId> next_metadata_id_{0};

  friend class SharedState<PoisonMetadataRecorder>;
  friend class PoisonMetadataRecorderTest;

  FRIEND_TEST_ALL_PREFIXES(PoisonMetadataRecorderTest, PoisonAlloc);
  FRIEND_TEST_ALL_PREFIXES(PoisonMetadataRecorderTest, SlotReuse);
  FRIEND_TEST_ALL_PREFIXES(
      ::gwp_asan::internal::LightweightDetectorAnalyzerTest,
      InternalError);
};

extern template class EXPORT_TEMPLATE_DECLARE(GWP_ASAN_EXPORT)
    SharedStateHolder<PoisonMetadataRecorder>;

}  // namespace gwp_asan::internal::lud

#endif  // COMPONENTS_GWP_ASAN_CLIENT_LIGHTWEIGHT_DETECTOR_POISON_METADATA_RECORDER_H_
