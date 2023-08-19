// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_handler/allocation_recorder_holder.h"

#include <utility>
#include <vector>

#include "base/debug/allocation_trace.h"
#include "build/build_config.h"
#include "components/allocation_recorder/internal/internal.h"
#include "components/allocation_recorder/testing/crashpad_fake_objects.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_architecture.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;
using crashpad::AnnotationSnapshot;
using crashpad::CPUArchitecture;
using crashpad::CPUContext;
using crashpad::test::TestExceptionSnapshot;
using crashpad::test::TestModuleSnapshot;
using crashpad::test::TestProcessMemory;
using crashpad::test::TestProcessSnapshot;

namespace allocation_recorder::crash_handler {

class AllocationTraceRecorderHolderTest : public ::testing::Test {
 protected:
  AllocationRecorderHolder& GetHolder() const { return *holder_; }

  AllocationTraceRecorder& GetAllocationTraceRecorder() const {
    return *allocation_trace_recorder_;
  }

  // Create a vector whose content represents the address of an object.
  std::vector<uint8_t> GetAddressData(const void* ptr) const;
  std::vector<uint8_t> GetAllocationRecorderAddressData() const;

  CPUContext CreateCPUContext(CPUArchitecture architecture) const;
  CPUContext CreateValidCPUContext() const;

  std::unique_ptr<TestExceptionSnapshot> CreateExceptionSnapshot(
      const CPUContext& cpu_context) const;
  std::unique_ptr<TestExceptionSnapshot> CreateValidExceptionSnapshot() const {
    return CreateExceptionSnapshot(CreateValidCPUContext());
  }

  // Verify the passed results are valid.
  // |ExpectAllocationTraceRecorderCreatedByTest| Expect that the recorder
  // is the instance created by the test suite. If false, just check that the
  // recorder is not null.
  template <bool ExpectAllocationTraceRecorderCreatedByTest = true>
  void VerifyIsValidSuccess(const Result& result) const;
  void VerifyIsValidError(const Result& result) const;

  // Verify that erroneous annotionations lead to an error.
  void VerifyAnnotationError(
      const std::vector<AnnotationSnapshot>& annotation_snapshots);

 private:
  const std::unique_ptr<AllocationTraceRecorder> allocation_trace_recorder_ =
      std::make_unique<AllocationTraceRecorder>();
  const scoped_refptr<AllocationRecorderHolder> holder_ =
      base::MakeRefCounted<AllocationRecorderHolder>();
};

std::vector<uint8_t> AllocationTraceRecorderHolderTest::GetAddressData(
    const void* ptr) const {
  return {reinterpret_cast<uint8_t*>(&ptr),
          reinterpret_cast<uint8_t*>(&ptr) + sizeof(ptr)};
}

std::vector<uint8_t>
AllocationTraceRecorderHolderTest::GetAllocationRecorderAddressData() const {
  auto& allocation_trace_recorder = GetAllocationTraceRecorder();

  return GetAddressData(&allocation_trace_recorder);
}

CPUContext AllocationTraceRecorderHolderTest::CreateCPUContext(
    CPUArchitecture architecture) const {
  return {architecture, {nullptr}};
}

std::unique_ptr<TestExceptionSnapshot>
AllocationTraceRecorderHolderTest::CreateExceptionSnapshot(
    const CPUContext& cpu_context) const {
  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot =
      std::make_unique<TestExceptionSnapshot>();

  (*test_exception_snapshot->MutableContext()) = cpu_context;

  return test_exception_snapshot;
}

CPUContext AllocationTraceRecorderHolderTest::CreateValidCPUContext() const {
#if defined(ARCH_CPU_ARM64)
  return CreateCPUContext(::crashpad::kCPUArchitectureARM64);
#elif defined(ARCH_CPU_X86_64)
  return CreateCPUContext(::crashpad::kCPUArchitectureX86_64);
#else
#error "Unsupported CPU architecture."
#endif
}

template <bool ExpectExactAllocationTraceRecorder>
void AllocationTraceRecorderHolderTest::VerifyIsValidSuccess(
    const Result& result) const {
  EXPECT_TRUE(result.has_value());
  if (ExpectExactAllocationTraceRecorder) {
    auto& allocation_trace_recorder = GetAllocationTraceRecorder();
    EXPECT_EQ(result.value(), &allocation_trace_recorder);
  } else {
    EXPECT_NE(result.value(), nullptr);
  }
}

void AllocationTraceRecorderHolderTest::VerifyIsValidError(
    const Result& result) const {
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().empty(), false);
}

// Verify that an error result is returned when the given annotation snapshots
// are used.
void AllocationTraceRecorderHolderTest::VerifyAnnotationError(
    const std::vector<::crashpad::AnnotationSnapshot>& annotation_snapshots) {
  std::unique_ptr<TestModuleSnapshot> test_module_snapshot =
      std::make_unique<TestModuleSnapshot>();
  test_module_snapshot->SetAnnotationObjects(annotation_snapshots);

  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot =
      std::make_unique<TestExceptionSnapshot>();
  (*test_exception_snapshot->MutableContext()) = CreateValidCPUContext();

  TestProcessSnapshot test_process_snapshot;
  test_process_snapshot.AddModule(std::move(test_module_snapshot));
  test_process_snapshot.SetException(std::move(test_exception_snapshot));

  AllocationRecorderHolder& holder = GetHolder();

  Result result = holder.Initialize(test_process_snapshot);

  VerifyIsValidError(result);
}

// Verify that Initialize returns a correct result when all the information is
// provided as required.
TEST_F(AllocationTraceRecorderHolderTest, VerifyInitialize) {
  auto& allocation_trace_recorder = GetAllocationTraceRecorder();

  AnnotationSnapshot annotation_snapshot_crash{
      kAnnotationName, static_cast<uint16_t>(kAnnotationType),
      GetAllocationRecorderAddressData()};

  std::unique_ptr<TestModuleSnapshot> test_module_snapshot =
      std::make_unique<TestModuleSnapshot>();
  test_module_snapshot->SetAnnotationObjects({annotation_snapshot_crash});

  TestProcessMemory::CallbackType callback = base::BindRepeating(
      [](::crashpad::VMAddress address, size_t size, void* buffer) -> ssize_t {
        memcpy(buffer, reinterpret_cast<void*>(address), size);
        return size;
      });

  std::unique_ptr<TestProcessMemory> process_memory_mock =
      std::make_unique<TestProcessMemory>(callback);

  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot =
      CreateValidExceptionSnapshot();

  TestProcessSnapshot test_process_snapshot;
  test_process_snapshot.SetProcessMemory(std::move(process_memory_mock));
  test_process_snapshot.AddModule(std::move(test_module_snapshot));
  test_process_snapshot.SetException(std::move(test_exception_snapshot));

  AllocationRecorderHolder& holder = GetHolder();

  Result result = holder.Initialize(test_process_snapshot);

  VerifyIsValidSuccess<false>(result);
  EXPECT_EQ(memcmp(&allocation_trace_recorder, result.value(),
                   sizeof(base::debug::tracer::AllocationTraceRecorder)),
            0);
}

TEST_F(AllocationTraceRecorderHolderTest, VerifyInitializeNoAnnotation) {
  VerifyAnnotationError({});
}

// Verify an error is reported for an annotation with wrong name but correct
// type.
TEST_F(AllocationTraceRecorderHolderTest, VerifyInitializeWrongAnnotationName) {
  AnnotationSnapshot annotation_snapshot{
      kAnnotationName + std::string{"something_appended"},
      static_cast<uint16_t>(kAnnotationType),
      GetAllocationRecorderAddressData()};

  VerifyAnnotationError({annotation_snapshot});
}

// Verify an error is reported for an annotation with correct name but wrong
// type.
TEST_F(AllocationTraceRecorderHolderTest, VerifyInitializeWrongAnnotationType) {
  AnnotationSnapshot annotation_snapshot{
      kAnnotationName, static_cast<uint16_t>(kAnnotationType) + 8,
      GetAllocationRecorderAddressData()};

  VerifyAnnotationError({annotation_snapshot});
}

// Verify an error is reported for an annotation with correct name and type but
// wrong size of address data.
TEST_F(AllocationTraceRecorderHolderTest, VerifyInitializeWrongAnnotationSize) {
  std::vector<uint8_t> invalid_address_data =
      GetAllocationRecorderAddressData();
  invalid_address_data.emplace_back(1);

  AnnotationSnapshot annotation_snapshot{kAnnotationName,
                                         static_cast<uint16_t>(kAnnotationType),
                                         invalid_address_data};

  VerifyAnnotationError({annotation_snapshot});
}

// Verify an error is reported when no memory image is given.
TEST_F(AllocationTraceRecorderHolderTest, VerifyInitializeNoMemoryImage) {
  AnnotationSnapshot annotation_snapshot{kAnnotationName,
                                         static_cast<uint16_t>(kAnnotationType),
                                         GetAllocationRecorderAddressData()};

  std::unique_ptr<TestModuleSnapshot> test_module_snapshot =
      std::make_unique<TestModuleSnapshot>();
  test_module_snapshot->SetAnnotationObjects({annotation_snapshot});

  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot =
      std::make_unique<TestExceptionSnapshot>();
  (*test_exception_snapshot->MutableContext()) = CreateValidCPUContext();

  TestProcessSnapshot test_process_snapshot;
  test_process_snapshot.AddModule(std::move(test_module_snapshot));
  test_process_snapshot.SetException(std::move(test_exception_snapshot));

  AllocationRecorderHolder& holder = GetHolder();

  Result result = holder.Initialize(test_process_snapshot);

  VerifyIsValidError(result);
}

// Verify an error is reported when reading the client memory unsuccessfully.
TEST_F(AllocationTraceRecorderHolderTest,
       VerifyInitializeReadFromMemoryFailed) {
  AnnotationSnapshot annotation_snapshot{kAnnotationName,
                                         static_cast<uint16_t>(kAnnotationType),
                                         GetAllocationRecorderAddressData()};

  std::unique_ptr<TestModuleSnapshot> test_module_snapshot =
      std::make_unique<TestModuleSnapshot>();
  test_module_snapshot->SetAnnotationObjects({annotation_snapshot});

  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot =
      std::make_unique<TestExceptionSnapshot>();
  (*test_exception_snapshot->MutableContext()) = CreateValidCPUContext();

  std::unique_ptr<TestProcessMemory> process_memory_mock =
      std::make_unique<TestProcessMemory>(-1);

  TestProcessSnapshot test_process_snapshot;
  test_process_snapshot.SetProcessMemory(std::move(process_memory_mock));
  test_process_snapshot.AddModule(std::move(test_module_snapshot));
  test_process_snapshot.SetException(std::move(test_exception_snapshot));

  AllocationRecorderHolder& holder = GetHolder();

  Result result = holder.Initialize(test_process_snapshot);

  VerifyIsValidError(result);
}

// Verify an error is reported when encountering a 32- vs 64-bit mismatch.
TEST_F(AllocationTraceRecorderHolderTest,
       VerifyInitializeBitnessOfImageIsIncorrect) {
  std::unique_ptr<TestExceptionSnapshot> test_exception_snapshot;

#if defined(ARCH_CPU_64_BITS)
  test_exception_snapshot = CreateExceptionSnapshot(
      CreateCPUContext(::crashpad::kCPUArchitectureARM));
#else
  test_exception_snapshot = CreateExceptionSnapshot(
      CreateCPUContext(::crashpad::kCPUArchitectureARM64));
#endif

  TestProcessSnapshot test_process_snapshot;
  test_process_snapshot.SetException(std::move(test_exception_snapshot));

  AllocationRecorderHolder& holder = GetHolder();

  Result result = holder.Initialize(test_process_snapshot);

  VerifyIsValidError(result);
}
}  // namespace allocation_recorder::crash_handler
