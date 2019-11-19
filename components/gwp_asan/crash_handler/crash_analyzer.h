// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_
#define COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_

#include <stddef.h>

#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "third_party/crashpad/crashpad/util/misc/address_types.h"

namespace crashpad {
class ExceptionSnapshot;
class ProcessMemory;
class ProcessSnapshot;
}  // namespace crashpad

namespace gwp_asan {
namespace internal {

// Captures the result of the GWP-ASan crash analyzer, whether the crash is
// determined to be related or unrelated to GWP-ASan or if an error was
// encountered analyzing the exception.
//
// These values are persisted via UMA--entries should not be renumbered and
// numeric values should never be reused.
enum class GwpAsanCrashAnalysisResult {
  // The crash is not caused by GWP-ASan.
  kUnrelatedCrash = 0,
  // The crash is caused by GWP-ASan.
  kGwpAsanCrash = 1,
  // The ProcessMemory from the snapshot was null.
  kErrorNullProcessMemory = 2,
  // Failed to read the crashing process' memory of the global allocator.
  kErrorFailedToReadAllocator = 3,
  // The crashing process' global allocator members failed sanity checks.
  kErrorAllocatorFailedSanityCheck = 4,
  // Failed to read crash stack traces.
  kErrorFailedToReadStackTrace = 5,
  // The ExceptionSnapshot CPU context was null.
  kErrorNullCpuContext = 6,
  // The crashing process' bitness does not match the crash handler.
  kErrorMismatchedBitness = 7,
  // The allocator computed an invalid slot index.
  kErrorBadSlot = 8,
  // Failed to read the crashing process' memory of the SlotMetadata.
  kErrorFailedToReadSlotMetadata = 9,
  // The allocator stored an invalid metadata index for a given slot.
  kErrorBadMetadataIndex = 10,
  // The computed metadata index was outdated.
  kErrorOutdatedMetadataIndex = 11,
  // Failed to read the crashing process' slot to metadata mapping.
  kErrorFailedToReadSlotMetadataMapping = 12,
  // Number of values in this enumeration, required by UMA.
  kMaxValue = kErrorFailedToReadSlotMetadataMapping
};

class CrashAnalyzer {
 public:
  // Given a ProcessSnapshot, determine if the exception is related to GWP-ASan.
  // If it is, returns true and fill out the |proto| parameter with details
  // about the exception. Otherwise, returns false.
  static bool GetExceptionInfo(
      const crashpad::ProcessSnapshot& process_snapshot,
      gwp_asan::Crash* proto);

 private:
  using SlotMetadata = AllocatorState::SlotMetadata;

  // Given an ExceptionSnapshot, return the address of where the exception
  // occurred (or null if it was not a data access exception.)
  static crashpad::VMAddress GetAccessAddress(
      const crashpad::ExceptionSnapshot& exception);

  // If the allocator annotation is present in the given snapshot, then return
  // the address for the AllocatorState in the crashing process.
  static crashpad::VMAddress GetAllocatorAddress(
      const crashpad::ProcessSnapshot& process_snapshot,
      const char* annotation_name);

  // Given a snapshot and crash key, returns true if there was a valid
  // AllocatorState for the given allocator or false otherwise.
  static bool GetAllocatorState(
      const crashpad::ProcessSnapshot& process_snapshot,
      const char* crash_key,
      Crash_Allocator allocator,
      AllocatorState* state);

  // This method implements the underlying logic for GetExceptionInfo(). It
  // analyzes the AllocatorState of the crashing process, if the exception is
  // related to GWP-ASan it fills out the |proto| parameter and returns true.
  static bool AnalyzeCrashedAllocator(
      const crashpad::ProcessSnapshot& process_snapshot,
      const char* crash_key,
      Crash_Allocator allocator,
      gwp_asan::Crash* proto);

  // This method fills out an AllocationInfo protobuf from a stack trace
  // and a SlotMetadata::AllocationInfo struct.
  static void ReadAllocationInfo(const uint8_t* stack_trace,
                                 size_t stack_trace_offset,
                                 const SlotMetadata::AllocationInfo& slot_info,
                                 gwp_asan::Crash_AllocationInfo* proto_info);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CRASH_HANDLER_CRASH_ANALYZER_H_
