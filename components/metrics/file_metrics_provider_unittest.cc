// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/file_metrics_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {
const char kMetricsName[] = "TestMetrics";
const char kMetricsFilename[] = "file.metrics";
}  // namespace

namespace metrics {

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() {}

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    // Only remember locally created histograms; they have exactly 2 chars.
    if (strlen(histogram.histogram_name()) == 2)
      recorded_delta_histogram_names_.push_back(histogram.histogram_name());
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(HistogramFlattenerDeltaRecorder);
};


class FileMetricsProviderTest : public testing::TestWithParam<bool> {
 protected:
  const size_t kSmallFileSize = 64 << 10;  // 64 KiB
  const size_t kLargeFileSize =  2 << 20;  //  2 MiB

  enum : int { kMaxCreateHistograms = 10 };

  FileMetricsProviderTest()
      : create_large_files_(GetParam()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_),
        statistics_recorder_(
            base::StatisticsRecorder::CreateTemporaryForTesting()),
        prefs_(new TestingPrefServiceSimple) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileMetricsProvider::RegisterPrefs(prefs_->registry(), kMetricsName);
    FileMetricsProvider::SetTaskRunnerForTesting(task_runner_);
  }

  ~FileMetricsProviderTest() override {
    // Clear out any final remaining tasks.
    task_runner_->RunUntilIdle();
    FileMetricsProvider::SetTaskRunnerForTesting(nullptr);
    DCHECK_EQ(0U, filter_actions_remaining_);
    // If a global histogram allocator exists at this point then it likely
    // acquired histograms that will continue to point to the released
    // memory and potentially cause use-after-free memory corruption.
    DCHECK(!base::GlobalHistogramAllocator::Get());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  base::FilePath temp_dir() { return temp_dir_.GetPath(); }
  base::FilePath metrics_file() {
    return temp_dir_.GetPath().AppendASCII(kMetricsFilename);
  }

  FileMetricsProvider* provider() {
    if (!provider_)
      provider_.reset(new FileMetricsProvider(prefs()));
    return provider_.get();
  }

  void OnDidCreateMetricsLog() {
    provider()->OnDidCreateMetricsLog();
  }

  bool HasPreviousSessionData() { return provider()->HasPreviousSessionData(); }

  void MergeHistogramDeltas() {
    provider()->MergeHistogramDeltas();
  }

  bool HasIndependentMetrics() { return provider()->HasIndependentMetrics(); }

  bool ProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) {
    bool success = false;
    bool success_set = false;
    provider()->ProvideIndependentMetrics(
        base::BindOnce(
            [](bool* success_ptr, bool* set_ptr, bool s) {
              *success_ptr = s;
              *set_ptr = true;
            },
            &success, &success_set),
        uma_proto, snapshot_manager);

    RunTasks();
    CHECK(success_set);
    return success;
  }

  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) {
    provider()->RecordInitialHistogramSnapshots(snapshot_manager);
  }

  size_t GetSnapshotHistogramCount() {
    // Merge the data from the allocator into the StatisticsRecorder.
    provider()->MergeHistogramDeltas();

    // Flatten what is known to see what has changed since the last time.
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    // "true" to the begin() includes histograms held in persistent storage.
    base::StatisticsRecorder::PrepareDeltas(true, base::Histogram::kNoFlags,
                                            base::Histogram::kNoFlags,
                                            &snapshot_manager);
    return flattener.GetRecordedDeltaHistogramNames().size();
  }

  size_t GetIndependentHistogramCount() {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    ChromeUserMetricsExtension uma_proto;
    provider()->ProvideIndependentMetrics(base::BindOnce([](bool success) {}),
                                          &uma_proto, &snapshot_manager);

    RunTasks();
    return flattener.GetRecordedDeltaHistogramNames().size();
  }

  void CreateGlobalHistograms(int histogram_count) {
    DCHECK_GT(kMaxCreateHistograms, histogram_count);

    // Create both sparse and normal histograms in the allocator.
    created_histograms_[0] = base::SparseHistogram::FactoryGet("h0", 0);
    created_histograms_[0]->Add(0);
    for (int i = 1; i < histogram_count; ++i) {
      created_histograms_[i] = base::Histogram::FactoryGet(
          base::StringPrintf("h%d", i), 1, 100, 10, 0);
      created_histograms_[i]->Add(i);
    }
  }

  void RunTasks() {
    // Run pending tasks twice: Once for IPC calls, once for replies. Don't
    // use RunUntilIdle() because that can do more work than desired.
    task_runner_->RunPendingTasks();
    task_runner_->RunPendingTasks();
  }

  void WriteMetricsFile(const base::FilePath& path,
                        base::PersistentHistogramAllocator* metrics) {
    base::File writer(path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    // Use DCHECK so the stack-trace will indicate where this was called.
    DCHECK(writer.IsValid()) << path.value();
    size_t file_size = create_large_files_ ? metrics->size() : metrics->used();
    int written = writer.Write(0, (const char*)metrics->data(), file_size);
    DCHECK_EQ(static_cast<int>(file_size), written);
  }

  void WriteMetricsFileAtTime(const base::FilePath& path,
                              base::PersistentHistogramAllocator* metrics,
                              base::Time write_time) {
    WriteMetricsFile(path, metrics);
    base::TouchFile(path, write_time, write_time);
  }

  std::unique_ptr<base::PersistentHistogramAllocator>
  CreateMetricsFileWithHistograms(
      const base::FilePath& file_path,
      base::Time write_time,
      int histogram_count,
      base::OnceCallback<void(base::PersistentHistogramAllocator*)> callback) {
    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        create_large_files_ ? kLargeFileSize : kSmallFileSize,
        0, kMetricsName);

    CreateGlobalHistograms(histogram_count);

    std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
        base::GlobalHistogramAllocator::ReleaseForTesting();
    std::move(callback).Run(histogram_allocator.get());

    WriteMetricsFileAtTime(file_path, histogram_allocator.get(), write_time);
    return histogram_allocator;
  }

  std::unique_ptr<base::PersistentHistogramAllocator>
  CreateMetricsFileWithHistograms(int histogram_count) {
    return CreateMetricsFileWithHistograms(
        metrics_file(), base::Time::Now(), histogram_count,
        base::BindOnce([](base::PersistentHistogramAllocator* allocator) {}));
  }

  base::HistogramBase* GetCreatedHistogram(int index) {
    DCHECK_GT(kMaxCreateHistograms, index);
    return created_histograms_[index];
  }

  void SetFilterActions(FileMetricsProvider::Params* params,
                        const FileMetricsProvider::FilterAction* actions,
                        size_t count) {
    filter_actions_ = actions;
    filter_actions_remaining_ = count;
    params->filter = base::Bind(&FileMetricsProviderTest::FilterSourcePath,
                                base::Unretained(this));
  }

  const bool create_large_files_;

 private:
  FileMetricsProvider::FilterAction FilterSourcePath(
      const base::FilePath& path) {
    DCHECK_LT(0U, filter_actions_remaining_);
    --filter_actions_remaining_;
    return *filter_actions_++;
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<FileMetricsProvider> provider_;
  base::HistogramBase* created_histograms_[kMaxCreateHistograms];

  const FileMetricsProvider::FilterAction* filter_actions_ = nullptr;
  size_t filter_actions_remaining_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FileMetricsProviderTest);
};

// Run all test cases with both small and large files.
INSTANTIATE_TEST_SUITE_P(SmallAndLargeFiles,
                         FileMetricsProviderTest,
                         testing::Bool());

TEST_P(FileMetricsProviderTest, AccessMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time metrics_time = base::Time::Now() - base::TimeDelta::FromMinutes(5);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  // File should have been deleted but recreate it to test behavior should
  // the file not be deleteable by this process.
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator.get(),
                         metrics_time);

  // Second full run on the same file should produce nothing.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Recreate the file to indicate that it is "new" and must be recorded.
  metrics_time = metrics_time + base::TimeDelta::FromMinutes(1);
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator.get(),
                         metrics_time);

  // This run should again have "new" histograms.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessTimeLimitedFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time metrics_time = base::Time::Now() - base::TimeDelta::FromHours(5);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  params.max_age = base::TimeDelta::FromHours(1);
  provider()->RegisterSource(params);

  // Attempt to access the file should return nothing.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, FilterDelaysFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time now_time = base::Time::Now();
  base::Time metrics_time = now_time - base::TimeDelta::FromMinutes(5);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);
  base::File::Info fileinfo;
  ASSERT_TRUE(base::GetFileInfo(metrics_file(), &fileinfo));
  EXPECT_GT(base::Time::Now(), fileinfo.last_modified);

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  const FileMetricsProvider::FilterAction actions[] = {
      FileMetricsProvider::FILTER_TRY_LATER,
      FileMetricsProvider::FILTER_PROCESS_FILE};
  SetFilterActions(&params, actions, base::size(actions));
  provider()->RegisterSource(params);

  // Processing the file should touch it but yield no results. File timestamp
  // accuracy is limited so compare the touched time to a couple seconds past.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
  ASSERT_TRUE(base::GetFileInfo(metrics_file(), &fileinfo));
  EXPECT_LT(metrics_time, fileinfo.last_modified);
  EXPECT_LE(now_time - base::TimeDelta::FromSeconds(2), fileinfo.last_modified);

  // Second full run on the same file should process the file.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, FilterSkipsFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time now_time = base::Time::Now();
  base::Time metrics_time = now_time - base::TimeDelta::FromMinutes(5);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);
  base::File::Info fileinfo;
  ASSERT_TRUE(base::GetFileInfo(metrics_file(), &fileinfo));
  EXPECT_GT(base::Time::Now(), fileinfo.last_modified);

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  const FileMetricsProvider::FilterAction actions[] = {
      FileMetricsProvider::FILTER_SKIP_FILE};
  SetFilterActions(&params, actions, base::size(actions));
  provider()->RegisterSource(params);

  // Processing the file should delete it.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      64 << 10, 0, kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create files starting with a timestamp a few minutes back.
  base::Time base_time = base::Time::Now() - base::TimeDelta::FromMinutes(10);

  // Create some files in an odd order. The files are "touched" back in time to
  // ensure that each file has a later timestamp on disk than the previous one.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII(".foo.pma"),
                         allocator, base_time);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("_bar.pma"),
                         allocator, base_time);

  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("c2.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(2));

  histogram = base::Histogram::FactoryGet("h3", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b3.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(3));

  histogram = base::Histogram::FactoryGet("h4", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("d4.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(4));

  base::TouchFile(metrics_files.GetPath().AppendASCII("b3.pma"),
                  base_time + base::TimeDelta::FromMinutes(5),
                  base_time + base::TimeDelta::FromMinutes(5));

  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("baz"), allocator,
                         base_time + base::TimeDelta::FromMinutes(6));

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName));

  // Files could come out in the order: a1, c2, d4, b3. They are recognizeable
  // by the number of histograms contained within each.
  const uint32_t expect_order[] = {1, 2, 4, 3, 0};
  for (size_t i = 0; i < base::size(expect_order); ++i) {
    // Record embedded snapshots via snapshot-manager.
    OnDidCreateMetricsLog();
    RunTasks();
    EXPECT_EQ(expect_order[i], GetSnapshotHistogramCount()) << i;
  }

  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("c2.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("b3.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("d4.pma")));
  EXPECT_TRUE(
      base::PathExists(metrics_files.GetPath().AppendASCII(".foo.pma")));
  EXPECT_TRUE(
      base::PathExists(metrics_files.GetPath().AppendASCII("_bar.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("baz")));
}

TEST_P(FileMetricsProviderTest, AccessDirectoryWithInvalidFiles) {
  ASSERT_FALSE(PathExists(metrics_file()));

  // Create files starting with a timestamp a few minutes back.
  base::Time base_time = base::Time::Now() - base::TimeDelta::FromMinutes(10);

  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());

  CreateMetricsFileWithHistograms(
      metrics_files.GetPath().AppendASCII("h1.pma"),
      base_time + base::TimeDelta::FromMinutes(1), 1,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        allocator->memory_allocator()->SetMemoryState(
            base::PersistentMemoryAllocator::MEMORY_DELETED);
      }));

  CreateMetricsFileWithHistograms(
      metrics_files.GetPath().AppendASCII("h2.pma"),
      base_time + base::TimeDelta::FromMinutes(2), 2,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        SystemProfileProto profile_proto;
        SystemProfileProto::FieldTrial* trial = profile_proto.add_field_trial();
        trial->set_name_id(123);
        trial->set_group_id(456);

        PersistentSystemProfile persistent_profile;
        persistent_profile.RegisterPersistentAllocator(
            allocator->memory_allocator());
        persistent_profile.SetSystemProfile(profile_proto, true);
      }));

  CreateMetricsFileWithHistograms(
      metrics_files.GetPath().AppendASCII("h3.pma"),
      base_time + base::TimeDelta::FromMinutes(3), 3,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        allocator->memory_allocator()->SetMemoryState(
            base::PersistentMemoryAllocator::MEMORY_DELETED);
      }));

  {
    base::File empty(metrics_files.GetPath().AppendASCII("h4.pma"),
                     base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  base::TouchFile(metrics_files.GetPath().AppendASCII("h4.pma"),
                  base_time + base::TimeDelta::FromMinutes(4),
                  base_time + base::TimeDelta::FromMinutes(4));

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName));

  // No files yet.
  EXPECT_EQ(0U, GetIndependentHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h1.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h2.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h4.pma")));

  // H1 should be skipped and H2 available.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("h1.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h2.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h4.pma")));

  // H2 should be read and the file deleted.
  EXPECT_EQ(2U, GetIndependentHistogramCount());
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("h2.pma")));

  // Nothing else should be found but the last (valid but empty) file will
  // stick around to be processed later (should it get expanded).
  EXPECT_EQ(0U, GetIndependentHistogramCount());
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.GetPath().AppendASCII("h4.pma")));
}

TEST_P(FileMetricsProviderTest, AccessTimeLimitedDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(64 << 10, 0,
                                                        kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create one old file and one new file.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator,
                         base::Time::Now() - base::TimeDelta::FromHours(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b2.pma"),
                         allocator, base::Time::Now());

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  params.max_age = base::TimeDelta::FromMinutes(30);
  provider()->RegisterSource(params);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("b2.pma")));
}

TEST_P(FileMetricsProviderTest, AccessCountLimitedDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(64 << 10, 0,
                                                        kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create one old file and one new file.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator,
                         base::Time::Now() - base::TimeDelta::FromHours(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b2.pma"),
                         allocator, base::Time::Now());

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  params.max_dir_files = 1;
  provider()->RegisterSource(params);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("b2.pma")));
}

TEST_P(FileMetricsProviderTest, AccessSizeLimitedDirectory) {
  // This only works with large files that are big enough to count.
  if (!create_large_files_)
    return;

  ASSERT_FALSE(PathExists(metrics_file()));

  size_t file_size_kib = 64;
  base::GlobalHistogramAllocator::CreateWithLocalMemory(file_size_kib << 10, 0,
                                                        kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create one old file and one new file.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator,
                         base::Time::Now() - base::TimeDelta::FromHours(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b2.pma"),
                         allocator, base::Time::Now());

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  params.max_dir_kib = file_size_kib + 1;
  provider()->RegisterSource(params);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("b2.pma")));
}

TEST_P(FileMetricsProviderTest, AccessFilteredDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(64 << 10, 0,
                                                        kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create files starting with a timestamp a few minutes back.
  base::Time base_time = base::Time::Now() - base::TimeDelta::FromMinutes(10);

  // Create some files in an odd order. The files are "touched" back in time to
  // ensure that each file has a later timestamp on disk than the previous one.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());

  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("c2.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(2));

  histogram = base::Histogram::FactoryGet("h3", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b3.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(3));

  histogram = base::Histogram::FactoryGet("h4", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("d4.pma"),
                         allocator,
                         base_time + base::TimeDelta::FromMinutes(4));

  base::TouchFile(metrics_files.GetPath().AppendASCII("b3.pma"),
                  base_time + base::TimeDelta::FromMinutes(5),
                  base_time + base::TimeDelta::FromMinutes(5));

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_files.GetPath(),
      FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  const FileMetricsProvider::FilterAction actions[] = {
      FileMetricsProvider::FILTER_PROCESS_FILE,   // a1
      FileMetricsProvider::FILTER_TRY_LATER,      // c2
      FileMetricsProvider::FILTER_SKIP_FILE,      // d4
      FileMetricsProvider::FILTER_PROCESS_FILE,   // b3
      FileMetricsProvider::FILTER_PROCESS_FILE};  // c2 (again)
  SetFilterActions(&params, actions, base::size(actions));
  provider()->RegisterSource(params);

  // Files could come out in the order: a1, b3, c2. They are recognizeable
  // by the number of histograms contained within each.
  const uint32_t expect_order[] = {1, 3, 2, 0};
  for (size_t i = 0; i < base::size(expect_order); ++i) {
    // Record embedded snapshots via snapshot-manager.
    OnDidCreateMetricsLog();
    RunTasks();
    EXPECT_EQ(expect_order[i], GetSnapshotHistogramCount()) << i;
  }

  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("c2.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("b3.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.GetPath().AppendASCII("d4.pma")));
}

TEST_P(FileMetricsProviderTest, AccessReadWriteMetrics) {
  // Create a global histogram allocator that maps to a file.
  ASSERT_FALSE(PathExists(metrics_file()));
  base::GlobalHistogramAllocator::CreateWithFile(
      metrics_file(),
      create_large_files_ ? kLargeFileSize : kSmallFileSize,
      0, kMetricsName);
  CreateGlobalHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::HistogramBase* h0 = GetCreatedHistogram(0);
  base::HistogramBase* h1 = GetCreatedHistogram(1);
  DCHECK(h0);
  DCHECK(h1);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN));

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Change a histogram and ensure that it's counted.
  h0->Add(0);
  EXPECT_EQ(1U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Change the other histogram and verify.
  h1->Add(11);
  EXPECT_EQ(1U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessInitialMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN, kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  ASSERT_TRUE(HasPreviousSessionData());
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // A run for normal histograms should produce nothing.
  CreateMetricsFileWithHistograms(2);
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsWithoutProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    ChromeUserMetricsExtension uma_proto;

    // A read of metrics with internal profiles should return nothing.
    EXPECT_FALSE(HasIndependentMetrics());
    EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  }
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsWithProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        SystemProfileProto profile_proto;
        SystemProfileProto::FieldTrial* trial = profile_proto.add_field_trial();
        trial->set_name_id(123);
        trial->set_group_id(456);

        PersistentSystemProfile persistent_profile;
        persistent_profile.RegisterPersistentAllocator(
            allocator->memory_allocator());
        persistent_profile.SetSystemProfile(profile_proto, true);
      }));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());

    // A read of metrics with internal profiles should return one result.
    ChromeUserMetricsExtension uma_proto;
    EXPECT_TRUE(HasIndependentMetrics());
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
    EXPECT_FALSE(HasIndependentMetrics());
    EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  }
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedFallbackMetricsWithoutProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
      kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  ASSERT_TRUE(HasPreviousSessionData());
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());

    // A read of metrics with internal profiles should return nothing.
    ChromeUserMetricsExtension uma_proto;
    EXPECT_FALSE(HasIndependentMetrics());
    EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  }
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedFallbackMetricsWithProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        SystemProfileProto profile_proto;
        SystemProfileProto::FieldTrial* trial = profile_proto.add_field_trial();
        trial->set_name_id(123);
        trial->set_group_id(456);

        PersistentSystemProfile persistent_profile;
        persistent_profile.RegisterPersistentAllocator(
            allocator->memory_allocator());
        persistent_profile.SetSystemProfile(profile_proto, true);
      }));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(FileMetricsProvider::Params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
      kMetricsName));

  // Record embedded snapshots via snapshot-manager.
  EXPECT_FALSE(HasPreviousSessionData());
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());

    // A read of metrics with internal profiles should return one result.
    ChromeUserMetricsExtension uma_proto;
    EXPECT_TRUE(HasIndependentMetrics());
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
    EXPECT_FALSE(HasIndependentMetrics());
    EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  }
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsFromDir) {
  const int file_count = 3;
  base::Time file_base_time = base::Time::Now();
  std::vector<base::FilePath> file_names;
  for (int i = 0; i < file_count; ++i) {
    CreateMetricsFileWithHistograms(
        metrics_file(), base::Time::Now(), 2,
        base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
          SystemProfileProto profile_proto;
          SystemProfileProto::FieldTrial* trial =
              profile_proto.add_field_trial();
          trial->set_name_id(123);
          trial->set_group_id(456);

          PersistentSystemProfile persistent_profile;
          persistent_profile.RegisterPersistentAllocator(
              allocator->memory_allocator());
          persistent_profile.SetSystemProfile(profile_proto, true);
        }));
    ASSERT_TRUE(PathExists(metrics_file()));
    char new_name[] = "hX";
    new_name[1] = '1' + i;
    base::FilePath file_name = temp_dir().AppendASCII(new_name).AddExtension(
        base::PersistentMemoryAllocator::kFileExtension);
    base::Time file_time =
        file_base_time - base::TimeDelta::FromMinutes(file_count - i);
    base::TouchFile(metrics_file(), file_time, file_time);
    base::Move(metrics_file(), file_name);
    file_names.push_back(std::move(file_name));
  }

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(FileMetricsProvider::Params(
      temp_dir(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
      FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE));

  OnDidCreateMetricsLog();
  RunTasks();

  // A read of metrics with internal profiles should return one result.
  HistogramFlattenerDeltaRecorder flattener;
  base::HistogramSnapshotManager snapshot_manager(&flattener);
  ChromeUserMetricsExtension uma_proto;
  for (int i = 0; i < file_count; ++i) {
    EXPECT_TRUE(HasIndependentMetrics()) << i;
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager)) << i;
    RunTasks();
  }
  EXPECT_FALSE(HasIndependentMetrics());
  EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));

  OnDidCreateMetricsLog();
  RunTasks();
  for (const auto& file_name : file_names)
    EXPECT_FALSE(base::PathExists(file_name));
}

}  // namespace metrics
