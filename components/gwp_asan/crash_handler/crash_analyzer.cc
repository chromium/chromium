// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/pack_stack_trace.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <signal.h>
#elif defined(OS_MACOSX)
#include <mach/exception_types.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

// Report failure for a particular allocator's histogram.
void ReportHistogram(Crash_Allocator allocator,
                     GwpAsanCrashAnalysisResult result) {
  DCHECK_LE(result, GwpAsanCrashAnalysisResult::kMaxValue);

  switch (allocator) {
    case Crash_Allocator_MALLOC:
      UMA_HISTOGRAM_ENUMERATION("GwpAsan.CrashAnalysisResult.Malloc", result);
      break;

    case Crash_Allocator_PARTITIONALLOC:
      UMA_HISTOGRAM_ENUMERATION("GwpAsan.CrashAnalysisResult.PartitionAlloc",
                                result);
      break;

    default:
      DCHECK(false) << "Unknown allocator value!";
  }
}

}  // namespace

using GetMetadataReturnType = AllocatorState::GetMetadataReturnType;

bool CrashAnalyzer::GetExceptionInfo(
    const crashpad::ProcessSnapshot& process_snapshot,
    gwp_asan::Crash* proto) {
  if (AnalyzeCrashedAllocator(process_snapshot, kMallocCrashKey,
                              Crash_Allocator_MALLOC, proto)) {
    return true;
  }

  if (AnalyzeCrashedAllocator(process_snapshot, kPartitionAllocCrashKey,
                              Crash_Allocator_PARTITIONALLOC, proto)) {
    return true;
  }

  return false;
}

crashpad::VMAddress CrashAnalyzer::GetAccessAddress(
    const crashpad::ExceptionSnapshot& exception) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (exception.Exception() == SIGSEGV || exception.Exception() == SIGBUS)
    return exception.ExceptionAddress();
#elif defined(OS_MACOSX)
  if (exception.Exception() == EXC_BAD_ACCESS)
    return exception.ExceptionAddress();
#elif defined(OS_WIN)
  if (exception.Exception() == EXCEPTION_ACCESS_VIOLATION) {
    const std::vector<uint64_t>& codes = exception.Codes();
    if (codes.size() < 2)
      DLOG(FATAL) << "Exception array is too small! " << codes.size();
    else
      return codes[1];
  }
#else
#error "Unknown platform"
#endif

  return 0;
}

crashpad::VMAddress CrashAnalyzer::GetAllocatorAddress(
    const crashpad::ProcessSnapshot& process_snapshot,
    const char* annotation_name) {
  for (auto* module : process_snapshot.Modules()) {
    for (auto annotation : module->AnnotationObjects()) {
      if (annotation.name != annotation_name)
        continue;

      if (annotation.type !=
          static_cast<uint16_t>(crashpad::Annotation::Type::kString)) {
        DLOG(ERROR) << "Bad annotation type " << annotation.type;
        return 0;
      }

      std::string annotation_str(reinterpret_cast<char*>(&annotation.value[0]),
                                 annotation.value.size());
      uint64_t value;
      if (!base::HexStringToUInt64(annotation_str, &value))
        return 0;
      return value;
    }
  }

  return 0;
}

bool CrashAnalyzer::GetAllocatorState(
    const crashpad::ProcessSnapshot& process_snapshot,
    const char* crash_key,
    Crash_Allocator allocator,
    AllocatorState* state) {
  crashpad::VMAddress gpa_addr =
      GetAllocatorAddress(process_snapshot, crash_key);
  // If the annotation isn't present, GWP-ASan wasn't enabled for this
  // allocator.
  if (!gpa_addr)
    return false;

  const crashpad::ExceptionSnapshot* exception = process_snapshot.Exception();
  if (!exception)
    return false;

  if (!exception->Context()) {
    DLOG(ERROR) << "Missing crash CPU context information.";
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorNullCpuContext);
    return false;
  }

#if defined(ARCH_CPU_64_BITS)
  constexpr bool is_64_bit = true;
#else
  constexpr bool is_64_bit = false;
#endif

  // TODO(vtsyrklevich): Look at using crashpad's process_types to read the GPA
  // state bitness-independently.
  if (exception->Context()->Is64Bit() != is_64_bit) {
    DLOG(ERROR) << "Mismatched process bitness.";
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorMismatchedBitness);
    return false;
  }

  const crashpad::ProcessMemory* memory = process_snapshot.Memory();
  if (!memory) {
    DLOG(ERROR) << "Null ProcessMemory.";
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorNullProcessMemory);
    return false;
  }

  if (!memory->Read(gpa_addr, sizeof(*state), state)) {
    DLOG(ERROR) << "Failed to read AllocatorState from process.";
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorFailedToReadAllocator);
    return false;
  }

  if (!state->IsValid()) {
    DLOG(ERROR) << "Allocator sanity check failed!";
    ReportHistogram(
        allocator,
        GwpAsanCrashAnalysisResult::kErrorAllocatorFailedSanityCheck);
    return false;
  }

  return true;
}

bool CrashAnalyzer::AnalyzeCrashedAllocator(
    const crashpad::ProcessSnapshot& process_snapshot,
    const char* crash_key,
    Crash_Allocator allocator,
    gwp_asan::Crash* proto) {
  AllocatorState valid_state;
  if (!GetAllocatorState(process_snapshot, crash_key, allocator, &valid_state))
    return false;

  crashpad::VMAddress exception_addr =
      GetAccessAddress(*process_snapshot.Exception());
  if (valid_state.double_free_address)
    exception_addr = valid_state.double_free_address;
  else if (valid_state.free_invalid_address)
    exception_addr = valid_state.free_invalid_address;

  if (!exception_addr || !valid_state.PointerIsMine(exception_addr))
    return false;
  // All errors that occur below happen for an exception known to be related to
  // GWP-ASan so we fill out the protobuf on error as well and include an error
  // string.

  proto->set_region_start(valid_state.pages_base_addr);
  proto->set_region_size(valid_state.pages_end_addr -
                         valid_state.pages_base_addr);
  if (valid_state.free_invalid_address)
    proto->set_free_invalid_address(valid_state.free_invalid_address);
  // We overwrite this later if it should be false.
  proto->set_missing_metadata(true);
  proto->set_allocator(allocator);

  // Read the allocator's entire metadata array.
  auto metadata_arr = std::make_unique<AllocatorState::SlotMetadata[]>(
      valid_state.num_metadata);
  if (!process_snapshot.Memory()->Read(
          valid_state.metadata_addr,
          sizeof(AllocatorState::SlotMetadata) * valid_state.num_metadata,
          metadata_arr.get())) {
    proto->set_internal_error("Failed to read metadata.");
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorFailedToReadSlotMetadata);
    return true;
  }

  // Read the allocator's slot_to_metadata mapping.
  auto slot_to_metadata =
      std::make_unique<AllocatorState::MetadataIdx[]>(valid_state.total_pages);
  if (!process_snapshot.Memory()->Read(
          valid_state.slot_to_metadata_addr,
          sizeof(AllocatorState::MetadataIdx) * valid_state.total_pages,
          slot_to_metadata.get())) {
    proto->set_internal_error("Failed to read slot_to_metadata.");
    ReportHistogram(
        allocator,
        GwpAsanCrashAnalysisResult::kErrorFailedToReadSlotMetadataMapping);
    return true;
  }

  AllocatorState::MetadataIdx metadata_idx;
  std::string error;
  auto ret = valid_state.GetMetadataForAddress(
      exception_addr, metadata_arr.get(), slot_to_metadata.get(), &metadata_idx,
      &error);
  if (ret == GetMetadataReturnType::kErrorBadSlot)
    ReportHistogram(allocator, GwpAsanCrashAnalysisResult::kErrorBadSlot);
  if (ret == GetMetadataReturnType::kErrorBadMetadataIndex)
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorBadMetadataIndex);
  if (ret == GetMetadataReturnType::kErrorOutdatedMetadataIndex)
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorOutdatedMetadataIndex);
  if (!error.empty()) {
    proto->set_internal_error(error);
    return true;
  }

  if (ret == GetMetadataReturnType::kGwpAsanCrash) {
    SlotMetadata& metadata = metadata_arr[metadata_idx];
    AllocatorState::ErrorType error =
        valid_state.GetErrorType(exception_addr, metadata.alloc.trace_collected,
                                 metadata.dealloc.trace_collected);
    proto->set_missing_metadata(false);
    proto->set_error_type(static_cast<Crash_ErrorType>(error));
    proto->set_allocation_address(metadata.alloc_ptr);
    proto->set_allocation_size(metadata.alloc_size);
    if (metadata.alloc.tid != base::kInvalidThreadId ||
        metadata.alloc.trace_len)
      ReadAllocationInfo(metadata.stack_trace_pool, 0, metadata.alloc,
                         proto->mutable_allocation());
    if (metadata.dealloc.tid != base::kInvalidThreadId ||
        metadata.dealloc.trace_len)
      ReadAllocationInfo(metadata.stack_trace_pool, metadata.alloc.trace_len,
                         metadata.dealloc, proto->mutable_deallocation());
  }

  ReportHistogram(allocator, GwpAsanCrashAnalysisResult::kGwpAsanCrash);
  return true;
}

void CrashAnalyzer::ReadAllocationInfo(
    const uint8_t* stack_trace,
    size_t stack_trace_offset,
    const SlotMetadata::AllocationInfo& slot_info,
    gwp_asan::Crash_AllocationInfo* proto_info) {
  if (slot_info.tid != base::kInvalidThreadId)
    proto_info->set_thread_id(slot_info.tid);

  if (!slot_info.trace_len || !slot_info.trace_collected)
    return;

  if (slot_info.trace_len > AllocatorState::kMaxPackedTraceLength ||
      stack_trace_offset + slot_info.trace_len >
          AllocatorState::kMaxPackedTraceLength) {
    DLOG(ERROR) << "Stack trace length is corrupted: " << slot_info.trace_len;
    return;
  }

  uintptr_t unpacked_stack_trace[AllocatorState::kMaxPackedTraceLength];
  size_t unpacked_len =
      Unpack(stack_trace + stack_trace_offset, slot_info.trace_len,
             unpacked_stack_trace, AllocatorState::kMaxPackedTraceLength);
  if (!unpacked_len) {
    DLOG(ERROR) << "Failed to unpack stack trace.";
    return;
  }

  // On 32-bit platforms we can't copy directly into
  // proto_info->mutable_stack_trace()->mutable_data().
  proto_info->mutable_stack_trace()->Resize(unpacked_len, 0);
  uint64_t* output = proto_info->mutable_stack_trace()->mutable_data();
  for (size_t i = 0; i < unpacked_len; i++)
    output[i] = unpacked_stack_trace[i];
}

}  // namespace internal
}  // namespace gwp_asan
