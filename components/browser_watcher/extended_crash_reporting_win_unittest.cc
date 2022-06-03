// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/extended_crash_reporting.h"

#include <windows.h>

#include "base/command_line.h"
#include "base/debug/activity_analyzer.h"
#include "base/debug/activity_tracker.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/process/process.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/browser_watcher/activity_report.pb.h"
#include "components/browser_watcher/activity_report_extractor.h"
#include "components/browser_watcher/activity_tracker_annotation.h"
#include "components/browser_watcher/features.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"

namespace browser_watcher {

namespace {

crashpad::Annotation* GetActivtyTrackerAnnotation() {
  crashpad::AnnotationList* annotation_list = crashpad::AnnotationList::Get();
  for (auto it = annotation_list->begin(); it != annotation_list->end(); ++it) {
    if (strcmp(ActivityTrackerAnnotation::kAnnotationName, (*it)->name()) ==
            0 &&
        ActivityTrackerAnnotation::kAnnotationType == (*it)->type()) {
      return *it;
    }
  }

  return nullptr;
}

bool IsNullOrEmpty(crashpad::Annotation* annotation) {
  return annotation == nullptr || annotation->size() == 0;
}

bool IsNonEmpty(crashpad::Annotation* annotation) {
  return annotation != nullptr && annotation->size() > 0;
}

using base::debug::GlobalActivityAnalyzer;
using base::debug::GlobalActivityTracker;

constexpr uint32_t kExceptionCode = 42U;
constexpr uint32_t kExceptionFlagContinuable = 0U;

}  // namespace

class ExtendedCrashReportingTest : public testing::Test {
 public:
  ExtendedCrashReportingTest() {}
  ~ExtendedCrashReportingTest() override {
    GlobalActivityTracker* global_tracker = GlobalActivityTracker::Get();
    if (global_tracker) {
      global_tracker->ReleaseTrackerForCurrentThreadForTesting();
      delete global_tracker;
    }
  }

  void SetUp() override {
    testing::Test::SetUp();

    // Initialize the crash keys, which will also reset the activity tracker
    // annotation if it's been used in previous tests.
    crash_reporter::InitializeCrashKeysForTesting();
  }
  void TearDown() override {
    ExtendedCrashReporting::TearDownForTesting();
    crash_reporter::ResetCrashKeysForTesting();
    testing::Test::TearDown();
  }

  std::unique_ptr<GlobalActivityAnalyzer> CreateAnalyzer() {
    GlobalActivityTracker* tracker = GlobalActivityTracker::Get();
    EXPECT_TRUE(tracker);
    base::PersistentMemoryAllocator* tmp = tracker->allocator();

    return GlobalActivityAnalyzer::CreateWithAllocator(
        std::make_unique<base::PersistentMemoryAllocator>(
            const_cast<void*>(tmp->data()), tmp->size(), 0u, 0u, "Copy", true));
  }
};

TEST_F(ExtendedCrashReportingTest, DisabledByDefault) {
  EXPECT_EQ(nullptr, ExtendedCrashReporting::SetUpIfEnabled(
                         ExtendedCrashReporting::kBrowserProcess));
  EXPECT_EQ(nullptr, ExtendedCrashReporting::GetInstance());
}

TEST_F(ExtendedCrashReportingTest, SetUpIsEnabledByFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kExtendedCrashReportingFeature);

  ExtendedCrashReporting* reporting = ExtendedCrashReporting::SetUpIfEnabled(
      ExtendedCrashReporting::kBrowserProcess);
  EXPECT_NE(nullptr, reporting);
  EXPECT_EQ(reporting, ExtendedCrashReporting::GetInstance());
}

TEST_F(ExtendedCrashReportingTest, RecordsAnnotation) {
  // Make sure the annotation doesn't exist before initialization.
  crashpad::Annotation* annotation = GetActivtyTrackerAnnotation();
  EXPECT_TRUE(IsNullOrEmpty(annotation));

  ExtendedCrashReporting::SetUpForTesting();
  EXPECT_NE(nullptr, ExtendedCrashReporting::GetInstance());
  EXPECT_TRUE(IsNonEmpty(GetActivtyTrackerAnnotation()));
}

#if defined(ADDRESS_SANITIZER) && defined(OS_WIN)
// The test does not pass under WinASan. See crbug.com/809524.
#define MAYBE_CrashingTest DISABLED_CrashingTest
#else
#define MAYBE_CrashingTest CrashingTest
#endif
TEST_F(ExtendedCrashReportingTest, MAYBE_CrashingTest) {
  ExtendedCrashReporting::SetUpForTesting();
  ExtendedCrashReporting* extended_crash_reporting =
      ExtendedCrashReporting::GetInstance();
  ASSERT_NE(nullptr, extended_crash_reporting);

  // Raise an exception, then continue.
  __try {
    ::RaiseException(kExceptionCode, kExceptionFlagContinuable, 0U, nullptr);
  } __except (EXCEPTION_CONTINUE_EXECUTION) {
  }

  // Collect the report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(CreateAnalyzer(), &report));

  // Validate expectations.
  ASSERT_EQ(1, report.process_states_size());
  const ProcessState& process_state = report.process_states(0);
  ASSERT_EQ(1, process_state.threads_size());

  bool thread_found = false;
  for (const ThreadState& thread : process_state.threads()) {
    if (thread.thread_id() == ::GetCurrentThreadId()) {
      thread_found = true;
      ASSERT_TRUE(thread.has_exception());
      const Exception& exception = thread.exception();
      EXPECT_EQ(kExceptionCode, exception.code());
      EXPECT_NE(0ULL, exception.program_counter());
      EXPECT_NE(0ULL, exception.exception_address());
      EXPECT_NE(0LL, exception.time());
    }
  }
  ASSERT_TRUE(thread_found);
}

}  // namespace browser_watcher
