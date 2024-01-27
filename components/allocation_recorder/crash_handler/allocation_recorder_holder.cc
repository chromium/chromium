// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/allocation_recorder_holder.h"

#include <optional>
#include <sstream>
#include <string>

#include "base/check.h"
#include "build/build_config.h"
#include "components/allocation_recorder/internal/internal.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"
#include "third_party/crashpad/crashpad/util/misc/address_types.h"
#include "third_party/crashpad/crashpad/util/process/process_memory.h"

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;

namespace allocation_recorder::crash_handler {
namespace {

enum class BitnessType { Unknown, Bit32, Bit64 };

const crashpad::CPUContext* GetCPUContext(
    const crashpad::ProcessSnapshot& process_snapshot) {
  const auto* const exception_snapshot = process_snapshot.Exception();
  if (exception_snapshot) {
    return exception_snapshot->Context();
  }

  return nullptr;
}

BitnessType GetBitnessType(const crashpad::ProcessSnapshot& process_snapshot) {
  const auto* const cpu_context = GetCPUContext(process_snapshot);

  if (!cpu_context) {
    return BitnessType::Unknown;
  }

  if (cpu_context->Is64Bit()) {
    return BitnessType::Bit64;
  }

  return BitnessType::Bit32;
}

constexpr const char* GetBitnessDescriptor(BitnessType bitness) {
  switch (bitness) {
    case BitnessType::Bit32:
      return "32bit";
    case BitnessType::Bit64:
      return "64bit";
    case BitnessType::Unknown:
      return "unknown";
  }
}

// Read the address of the AllocationTraceRecorder from the annotations passed
// in the process_snapshot. Returns an optional containing the exact address.
// In case of an error, an empty optional is returned and details on the error
// will be written to the error_stream.
std::optional<crashpad::VMAddress> GetRecorderVMAddress(
    const crashpad::ProcessSnapshot& process_snapshot,
    std::ostream& error_stream) {
  for (const auto* module : process_snapshot.Modules()) {
    for (const auto& annotation : module->AnnotationObjects()) {
      if (annotation.name != kAnnotationName) {
        continue;
      }

      if (annotation.type != static_cast<uint16_t>(kAnnotationType)) {
        error_stream << "Bad annotation type! name='" << kAnnotationName
                     << "', found-type=" << annotation.type
                     << ", required-type="
                     << static_cast<uint16_t>(kAnnotationType);
        return {};
      }

      if (annotation.value.size() != sizeof(uint64_t)) {
        error_stream << "Bad annotation size! name='" << kAnnotationName
                     << "', found-size=" << annotation.value.size()
                     << ", required-size=" << sizeof(uint64_t);
        return {};
      }

      uint64_t const value =
          *reinterpret_cast<const uint64_t*>(annotation.value.data());

      return {value};
    }
  }

  error_stream << "No annotation found! required-name=" << kAnnotationName;

  return {};
}

// The actual initialization function. It resolves the address of the
// AllocationTraceRecorder from the process snapshot and copies the binary
// image into the passed recorder.
bool DoInitialize(const crashpad::ProcessSnapshot& process_snapshot,
                  AllocationTraceRecorder* const recorder,
                  std::ostream& error_stream) {
  const auto allocation_recorder_vm_address =
      GetRecorderVMAddress(process_snapshot, error_stream);

  if (!allocation_recorder_vm_address) {
    // GetRecorderVMAddress already fills in the eventual error details, so no
    // need to add more here.
    return false;
  }

  const crashpad::ProcessMemory* const memory = process_snapshot.Memory();
  if (!memory) {
    error_stream << "No ProcessMemory accessible!";
    return false;
  }

  if (!memory->Read(*allocation_recorder_vm_address, sizeof(*recorder),
                    recorder)) {
    error_stream
        << "Failed to read AllocationTraceRecorder from client-process.";
    return false;
  }

  return true;
}

// Perform a simple sanity check to ensure crash client and crash handler can
// work together.
bool CheckSanity(const crashpad::ProcessSnapshot& process_snapshot,
                 std::ostream& error_stream) {
#if defined(ARCH_CPU_64_BITS)
  constexpr auto ExpectedBitness = BitnessType::Bit64;
#else
  constexpr auto ExpectedBitness = BitnessType::Bit32;
#endif

  const auto bitness = GetBitnessType(process_snapshot);

  if (bitness != ExpectedBitness) {
    error_stream << "Wrong bitness! expected="
                 << GetBitnessDescriptor(ExpectedBitness)
                 << ", found=" << GetBitnessDescriptor(bitness);
    return false;
  }

  return true;
}
}  // namespace

AllocationRecorderHolder::~AllocationRecorderHolder() = default;

Result AllocationRecorderHolder::Initialize(
    const crashpad::ProcessSnapshot& process_snapshot) {
  static_assert(std::is_standard_layout<AllocationTraceRecorder>::value, "");

  memset(&buffer_, 0, sizeof(buffer_));
  AllocationTraceRecorder* allocation_recorder =
      reinterpret_cast<AllocationTraceRecorder*>(&buffer_);

  std::ostringstream error_stream;

  if (CheckSanity(process_snapshot, error_stream) &&
      DoInitialize(process_snapshot, allocation_recorder, error_stream)) {
    return base::ok(allocation_recorder);
  }

  return base::unexpected(error_stream.str());
}

}  // namespace allocation_recorder::crash_handler
