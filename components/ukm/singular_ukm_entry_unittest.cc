// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/singular_ukm_entry.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {
// Test UKM event.
using PageLoad = builders::PageLoad;

// Test metric values.
constexpr int64_t kTestCpuTime = 10;
constexpr int64_t kTestCpuTime2 = 15;
}  // namespace

class SingularUkmEntryTest : public testing::Test {
 public:
  SingularUkmEntryTest() : thread_("test_thread") {}

 protected:
  // Creates a SingularUkmEntry on |thread_| and the stores the UKMEntry on
  // this thread.
  template <typename UkmEntry>
  std::unique_ptr<SingularUkmEntry<UkmEntry>> CreateEntryOnThread(SourceId id) {
    DCHECK(thread_.IsRunning());

    std::unique_ptr<SingularUkmEntry<UkmEntry>> entry;
    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&SingularUkmEntryTest::CreateAndStoreEntry<UkmEntry>,
                       &entry, id),
        run_loop.QuitClosure());
    run_loop.Run();
    return entry;
  }

  // Helper function to create and store an UKMEntry.
  template <typename UkmEntry>
  static void CreateAndStoreEntry(
      std::unique_ptr<SingularUkmEntry<UkmEntry>>* entry,
      SourceId id) {
    *entry = SingularUkmEntry<UkmEntry>::Create(id);
  }

  void StartThread() { ASSERT_TRUE(thread_.Start()); }

  void ShutdownThread() { thread_.Stop(); }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::Thread thread_;
};

TEST_F(SingularUkmEntryTest, InterfaceReceivesEntryFromBuilder) {
  TestAutoSetUkmRecorder ukm_recorder;

  // Create an UKMEntry on this thread.
  SourceId id1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  std::unique_ptr<SingularUkmEntry<PageLoad>> event1 =
      SingularUkmEntry<PageLoad>::Create(id1);

  {
    SingularUkmEntry<PageLoad>::EntryBuilder builder = event1->Builder();
    builder->SetCpuTime(1);
    // Destruction of the builder will submit the new entry to be stored.
  }

  {
    SingularUkmEntry<PageLoad>::EntryBuilder builder = event1->Builder();
    builder->SetCpuTime(2);
    // This will send the new entry and overwrite the previously stored entry.
  }

  {
    SingularUkmEntry<PageLoad>::EntryBuilder builder = event1->Builder();
    builder->SetCpuTime(kTestCpuTime);
    // This entry will be last sent entry and will be the entry that is
    // recorded.
  }

  // Resetting the entry will close the connection. During destruction of the
  // interface, the entry will be recorded by UKM.
  event1.reset();

  base::RunLoop().RunUntilIdle();

  // Expect only the last UkmEntry to be recorded by UKM.
  EXPECT_EQ(ukm_recorder.entries_count(), 1u);

  // Collect the metrics recorded.
  const auto metrics = ukm_recorder.FilteredHumanReadableMetricForEntry(
      PageLoad::kEntryName, PageLoad::kCpuTimeName);

  // Check the number and value of the found metrics.
  ASSERT_EQ(metrics.size(), 1u);
  ASSERT_EQ(metrics[0].size(), 1u);
  EXPECT_EQ(metrics[0].find(PageLoad::kCpuTimeName)->second, kTestCpuTime);
}

// Test that SingularUkmEntry's work from multiple threads. A SingularUkmEntry
// is created on this thread and a second thread. The second thread's
// SingularUkmEntry will create an EntryBuilder on this thread. This is expected
// to fail due to not being used on the same sequence it was created. Finally,
// both entries will create an UkmEntry on their respective thread. This will
// result in two UkmEntries with one metric each to be recorded once the entries
// are destroyed.
TEST_F(SingularUkmEntryTest, MultithreadedInterface) {
  // Allow EXPECT_DCHECK_DEATH for multiple threads.
  // https://github.com/google/googletest/blob/main/docs/advanced.md#death-tests-and-threads
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  TestAutoSetUkmRecorder ukm_recorder;

  // Create to two test SourceIds.
  const SourceId id1 = ConvertToSourceId(1, SourceIdType::NAVIGATION_ID);
  const SourceId id2 = ConvertToSourceId(2, SourceIdType::NAVIGATION_ID);

  // Create a SingularUkmEntry on the current thread.
  std::unique_ptr<SingularUkmEntry<PageLoad>> test_entry =
      SingularUkmEntry<PageLoad>::Create(id1);

  // Create a new thread.
  StartThread();

  // Creates a SingularUkmEntry on the new thread.
  std::unique_ptr<SingularUkmEntry<PageLoad>> thread_entry =
      CreateEntryOnThread<PageLoad>(id2);

  // Try to create a UkmBuilder. Expects this to fail since it is on the test's
  // sequence.
  EXPECT_DCHECK_DEATH(thread_entry->Builder());

  // Correctly create UKMEntries for both SingularUkmEntries.
  {
    // Create EntryBuilder for the test's thread.
    SingularUkmEntry<PageLoad>::EntryBuilder builder = test_entry->Builder();
    builder->SetCpuTime(kTestCpuTime);

    // Create a different EntryBuilder for the new thread.
    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReply(
        FROM_HERE, base::BindLambdaForTesting([&thread_entry]() {
          SingularUkmEntry<PageLoad>::EntryBuilder builder =
              thread_entry->Builder();
          builder->SetCpuTime(kTestCpuTime2);
        }),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Delete the entry on the thread for it to be recorded.
  thread_.task_runner()->DeleteSoon(FROM_HERE, thread_entry.release());
  ShutdownThread();

  // Delete entry so it is recorded.
  test_entry.reset();
  base::RunLoop().RunUntilIdle();

  // Find the expected metrics.
  const auto metrics = ukm_recorder.FilteredHumanReadableMetricForEntry(
      PageLoad::kEntryName, PageLoad::kCpuTimeName);

  // Check the number and value of the found metrics.
  ASSERT_EQ(metrics.size(), 2u);
  EXPECT_THAT(metrics,
              testing::UnorderedElementsAre(
                  testing::Eq<TestAutoSetUkmRecorder::HumanReadableUkmMetrics>(
                      {{PageLoad::kCpuTimeName, kTestCpuTime}}),
                  testing::Eq<TestAutoSetUkmRecorder::HumanReadableUkmMetrics>(
                      {{PageLoad::kCpuTimeName, kTestCpuTime2}})));
}

}  // namespace ukm
