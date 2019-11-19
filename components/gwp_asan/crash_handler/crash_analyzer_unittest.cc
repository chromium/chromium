// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/debug/stack_trace.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"
#include "third_party/crashpad/crashpad/test/process_type.h"
#include "third_party/crashpad/crashpad/util/process/process_memory_native.h"

namespace gwp_asan {
namespace internal {

namespace {

constexpr const char* kMallocHistogramName =
    "GwpAsan.CrashAnalysisResult.Malloc";
constexpr const char* kPartitionAllocHistogramName =
    "GwpAsan.CrashAnalysisResult.PartitionAlloc";

}  // namespace

class CrashAnalyzerTest : public testing::Test {
 protected:
  void SetUp() final {
    gpa_.Init(1, 1, 1, base::DoNothing(), false);
    InitializeSnapshot();
  }

  // Initializes the ProcessSnapshot so that it appears the given allocator was
  // used for backing malloc.
  void InitializeSnapshot() {
    std::string crash_key_value = gpa_.GetCrashKey();
    std::vector<uint8_t> crash_key_vector(crash_key_value.begin(),
                                          crash_key_value.end());

    std::vector<crashpad::AnnotationSnapshot> annotations;
    annotations.emplace_back(
        kMallocCrashKey,
        static_cast<uint16_t>(crashpad::Annotation::Type::kString),
        crash_key_vector);

    auto module = std::make_unique<crashpad::test::TestModuleSnapshot>();
    module->SetAnnotationObjects(annotations);

    auto exception = std::make_unique<crashpad::test::TestExceptionSnapshot>();
    // Just match the bitness, the actual architecture doesn't matter.
#if defined(ARCH_CPU_64_BITS)
    exception->MutableContext()->architecture =
        crashpad::CPUArchitecture::kCPUArchitectureX86_64;
#else
    exception->MutableContext()->architecture =
        crashpad::CPUArchitecture::kCPUArchitectureX86;
#endif

    auto memory = std::make_unique<crashpad::ProcessMemoryNative>();
    ASSERT_TRUE(memory->Initialize(crashpad::test::GetSelfProcess()));

    process_snapshot_.AddModule(std::move(module));
    process_snapshot_.SetException(std::move(exception));
    process_snapshot_.SetProcessMemory(std::move(memory));
  }

  GuardedPageAllocator gpa_;
  crashpad::test::TestProcessSnapshot process_snapshot_;
};

TEST_F(CrashAnalyzerTest, StackTraceCollection) {
  void* ptr = gpa_.Allocate(10);
  ASSERT_NE(ptr, nullptr);
  gpa_.Deallocate(ptr);

  // Lets pretend a double free() occurred on the allocation we saw previously.
  gpa_.state_.double_free_address = reinterpret_cast<uintptr_t>(ptr);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result = static_cast<int>(GwpAsanCrashAnalysisResult::kGwpAsanCrash);
  EXPECT_THAT(histogram_tester.GetAllSamples(kMallocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kPartitionAllocHistogramName, 0);

  ASSERT_TRUE(proto.has_allocation());
  ASSERT_TRUE(proto.has_deallocation());

  base::debug::StackTrace st;
  size_t trace_len;
  const void* const* trace = st.Addresses(&trace_len);
  ASSERT_NE(trace, nullptr);
  ASSERT_GT(trace_len, 0U);

  // Adjust the stack trace to point to the entry above the current frame.
  while (trace_len > 0) {
    if (trace[0] == __builtin_return_address(0))
      break;

    trace++;
    trace_len--;
  }

  ASSERT_GT(proto.allocation().stack_trace_size(), (int)trace_len);
  ASSERT_GT(proto.deallocation().stack_trace_size(), (int)trace_len);

  // Ensure that the allocation and deallocation stack traces match the stack
  // frames that we collected above the current frame.
  for (size_t i = 1; i <= trace_len; i++) {
    SCOPED_TRACE(i);
    ASSERT_EQ(proto.allocation().stack_trace(
                  proto.allocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
    ASSERT_EQ(proto.deallocation().stack_trace(
                  proto.deallocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
  }
}

TEST_F(CrashAnalyzerTest, InternalError) {
  // Lets pretend an invalid free() occurred in the allocator region.
  gpa_.state_.free_invalid_address =
      reinterpret_cast<uintptr_t>(gpa_.state_.first_page_addr);
  // Out of bounds slot_to_metadata_idx, allocator was initialized with only a
  // single entry slot/metadata entry.
  gpa_.slot_to_metadata_idx_[0] = 5;

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result =
      static_cast<int>(GwpAsanCrashAnalysisResult::kErrorBadMetadataIndex);
  EXPECT_THAT(histogram_tester.GetAllSamples(kMallocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kPartitionAllocHistogramName, 0);

  EXPECT_TRUE(proto.has_internal_error());
  ASSERT_TRUE(proto.has_missing_metadata());
  EXPECT_TRUE(proto.missing_metadata());
}

}  // namespace internal
}  // namespace gwp_asan
