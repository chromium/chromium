// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
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
#include "third_party/crashpad/crashpad/snapshot/cpu_architecture.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <signal.h>
#elif BUILDFLAG(IS_APPLE)
#include <mach/exception_types.h>
#elif BUILDFLAG(IS_WIN)
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
      UMA_HISTOGRAM_ENUMERATION("Security.GwpAsan.CrashAnalysisResult.Malloc",
                                result);
      break;

    case Crash_Allocator_PARTITIONALLOC:
      UMA_HISTOGRAM_ENUMERATION(
          "Security.GwpAsan.CrashAnalysisResult.PartitionAlloc", result);
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

  if (AnalyzeLightweightDetectorCrash(process_snapshot, proto)) {
    return true;
  }

  return false;
}

crashpad::VMAddress CrashAnalyzer::GetAccessAddress(
    const crashpad::ExceptionSnapshot& exception) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  if (exception.Exception() == SIGSEGV || exception.Exception() == SIGBUS)
    return exception.ExceptionAddress();
#elif BUILDFLAG(IS_APPLE)
  if (exception.Exception() == EXC_BAD_ACCESS)
    return exception.ExceptionAddress();
#elif BUILDFLAG(IS_WIN)
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

crashpad::VMAddress CrashAnalyzer::GetStateAddress(
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

template <typename T>
bool CrashAnalyzer::GetState(const crashpad::ProcessSnapshot& process_snapshot,
                             const char* crash_key,
                             Crash_Allocator allocator,
                             T* state) {
  crashpad::VMAddress state_addr = GetStateAddress(process_snapshot, crash_key);
  // If the annotation isn't present, GWP-ASan wasn't enabled for this
  // allocator.
  if (!state_addr) {
    return false;
  }

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

  if (!memory->Read(state_addr, sizeof(*state), state)) {
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

bool CrashAnalyzer::AnalyzeLightweightDetectorCrash(
    const crashpad::ProcessSnapshot& process_snapshot,
    gwp_asan::Crash* proto) {
  LightweightDetectorState valid_state;
  // TODO(glazunov): Add LUD to the `Allocator` enum. It is no longer
  // bound to PartitionAlloc.
  if (!GetState(process_snapshot, kLightweightDetectorCrashKey,
                Crash_Allocator_PARTITIONALLOC, &valid_state)) {
    return false;
  }

  auto mode = LightweightDetectorModeToGwpAsanMode(valid_state.mode);
  auto SetError = [proto, mode](const std::string& message) {
    proto->set_mode(mode);
    proto->set_missing_metadata(true);
    proto->set_internal_error(message);
  };

  auto* exception = process_snapshot.Exception();
  if (!exception->Context()->Is64Bit()) {
    // The lightweight detector isn't used on 32-bit platforms.
    return false;
  }

  size_t slot_count = valid_state.num_metadata;
  auto metadata_arr =
      std::make_unique<LightweightDetectorState::SlotMetadata[]>(slot_count);
  if (!process_snapshot.Memory()->Read(
          valid_state.metadata_addr,
          sizeof(LightweightDetectorState::SlotMetadata) * slot_count,
          metadata_arr.get())) {
    ReportHistogram(
        Crash_Allocator_PARTITIONALLOC,
        GwpAsanCrashAnalysisResult::kErrorFailedToReadLightweightSlotMetadata);
    SetError("Failed to read lightweight metadata.");
    return true;
  }

  bool seen_candidate_id = false;
  std::optional<LightweightDetectorState::MetadataId> metadata_id;
  std::vector<uint64_t> candidate_addresses;

#if defined(ARCH_CPU_X86_64)
  if (exception->Context()->architecture != crashpad::kCPUArchitectureX86_64) {
    ReportHistogram(
        Crash_Allocator_PARTITIONALLOC,
        GwpAsanCrashAnalysisResult::kErrorMismatchedCpuArchitecture);
    DLOG(ERROR) << "Mismatched CPU architecture.";
    return false;
  }

  // x86-64 CPUs won't report the exact access address if it's non-canonical.
  // Use a set of platform-specific hints to detect when it's the case
  // and attempt to extract the ID from the register values at the time of the
  // crash. See also "Intel 64 and IA-32 Architectures Software Developer’s
  // Manual", Volume 1, Section 3.3.7.1.
  if (
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
      // https://elixir.bootlin.com/linux/v6.2.2/source/arch/x86/kernel/traps.c#L719
      exception->Exception() == SIGSEGV &&
      exception->ExceptionInfo() == SI_KERNEL
#elif BUILDFLAG(IS_MAC)
      // https://opensource.apple.com/source/xnu/xnu-1699.24.8/osfmk/i386/trap.c
      exception->Exception() == EXC_BAD_ACCESS &&
      exception->ExceptionInfo() == EXC_I386_GPFLT
#elif BUILDFLAG(IS_WIN)
      // Verified experimentally.
      GetAccessAddress(*exception) == std::numeric_limits<uint64_t>::max()
#endif  // BUILDFLAG(IS_WIN)
  ) {
    auto& context = *exception->Context()->x86_64;
    candidate_addresses = {context.rax, context.rbx, context.rcx, context.rdx,
                           context.rdi, context.rsi, context.rbp, context.rsp,
                           context.r8,  context.r9,  context.r10, context.r11,
                           context.r12, context.r13, context.r14, context.r15,
                           context.rip};
  }
#else   // defined(ARCH_CPU_X86_64)
  candidate_addresses = {GetAccessAddress(*exception)};
#endif  // defined(ARCH_CPU_X86_64)

  for (auto candidate_address : candidate_addresses) {
    auto candidate_id =
        LightweightDetectorState::ExtractMetadataId(candidate_address);
    if (!candidate_id.has_value()) {
      continue;
    }
    seen_candidate_id = true;

    if (valid_state.HasMetadataForId(*candidate_id, metadata_arr.get())) {
      if (!metadata_id.has_value()) {
        // It's the first time we see an ID with a matching valid slot.
        metadata_id = candidate_id;
      } else if (metadata_id != candidate_id) {
        ReportHistogram(Crash_Allocator_PARTITIONALLOC,
                        GwpAsanCrashAnalysisResult::
                            kErrorConflictingLightweightMetadataIds);
        SetError("Found conflicting lightweight metadata IDs.");
        return true;
      }
    }
  }

  if (!seen_candidate_id) {
    return false;
  }

  if (!metadata_id.has_value()) {
    ReportHistogram(Crash_Allocator_PARTITIONALLOC,
                    GwpAsanCrashAnalysisResult::
                        kErrorInvalidOrOutdatedLightweightMetadataIndex);
    SetError(
        "The computed lightweight metadata index was invalid or outdated.");
    return true;
  }

  auto& metadata =
      valid_state.GetSlotMetadataById(*metadata_id, metadata_arr.get());

  proto->set_mode(mode);
  proto->set_missing_metadata(false);
  proto->set_allocator(Crash_Allocator_PARTITIONALLOC);
  proto->set_error_type(Crash_ErrorType_USE_AFTER_FREE);
  proto->set_allocation_address(metadata.alloc_ptr);
  proto->set_allocation_size(metadata.alloc_size);
  if (metadata.dealloc.tid != base::kInvalidThreadId ||
      metadata.dealloc.trace_len) {
    ReadAllocationInfo(metadata.deallocation_stack_trace,
                       /* stack_trace_offset = */ 0, metadata.dealloc,
                       proto->mutable_deallocation());
  }

  ReportHistogram(Crash_Allocator_PARTITIONALLOC,
                  GwpAsanCrashAnalysisResult::kLightweightDetectorCrash);
  return true;
}

bool CrashAnalyzer::AnalyzeCrashedAllocator(
    const crashpad::ProcessSnapshot& process_snapshot,
    const char* crash_key,
    Crash_Allocator allocator,
    gwp_asan::Crash* proto) {
  AllocatorState valid_state;
  if (!GetState(process_snapshot, crash_key, allocator, &valid_state)) {
    return false;
  }

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
  proto->set_mode(Crash_Mode_CLASSIC);
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
  auto slot_to_metadata = std::make_unique<AllocatorState::MetadataIdx[]>(
      valid_state.total_reserved_pages);
  if (!process_snapshot.Memory()->Read(valid_state.slot_to_metadata_addr,
                                       sizeof(AllocatorState::MetadataIdx) *
                                           valid_state.total_reserved_pages,
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
  if (ret == GetMetadataReturnType::kErrorBadSlot) {
    ReportHistogram(allocator, GwpAsanCrashAnalysisResult::kErrorBadSlot);
  }
  if (ret == GetMetadataReturnType::kErrorBadMetadataIndex) {
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorBadMetadataIndex);
  }
  if (ret == GetMetadataReturnType::kErrorOutdatedMetadataIndex) {
    ReportHistogram(allocator,
                    GwpAsanCrashAnalysisResult::kErrorOutdatedMetadataIndex);
  }
  if (!error.empty()) {
    proto->set_internal_error(error);
    return true;
  }

  if (ret == GetMetadataReturnType::kGwpAsanCrash) {
    AllocatorState::SlotMetadata& metadata = metadata_arr[metadata_idx];
    AllocatorState::ErrorType error_type =
        valid_state.GetErrorType(exception_addr, metadata.alloc.trace_collected,
                                 metadata.dealloc.trace_collected);
    proto->set_missing_metadata(false);
    proto->set_error_type(static_cast<Crash_ErrorType>(error_type));
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
    const AllocationInfo& slot_info,
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

Crash_Mode CrashAnalyzer::LightweightDetectorModeToGwpAsanMode(
    LightweightDetectorMode mode) {
  switch (mode) {
    case LightweightDetectorMode::kBrpQuarantine:
      return Crash_Mode_LIGHTWEIGHT_DETECTOR_BRP;
    case LightweightDetectorMode::kRandom:
      return Crash_Mode_LIGHTWEIGHT_DETECTOR_RANDOM;
    default:
      return Crash_Mode_UNSPECIFIED;
  }
}

}  // namespace internal
}  // namespace gwp_asan
