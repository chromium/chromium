// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/metrics/file_metrics_provider.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {
const char kMetricsName[] = "TestMetrics";
const char kMergedCountHistogramName[] =
    "UMA.FileMetricsProvider.TestMetrics.MergedHistogramsCount";
const char kMetricsFilename[] = "file.metrics";

void WriteSystemProfileToAllocator(
    base::PersistentHistogramAllocator* allocator) {
  metrics::SystemProfileProto profile_proto;
  // Add a field trial to verify that FileMetricsProvider will produce an
  // independent log with the written system profile. Similarly for the session
  // hash.
  metrics::SystemProfileProto::FieldTrial* trial =
      profile_proto.add_field_trial();
  trial->set_name_id(123);
  trial->set_group_id(456);
  profile_proto.set_session_hash(789);
  metrics::PersistentSystemProfile persistent_profile;
  persistent_profile.RegisterPersistentAllocator(allocator->memory_allocator());
  persistent_profile.SetSystemProfile(profile_proto, /*complete=*/true);
}
}  // namespace

namespace metrics {

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() = default;

  HistogramFlattenerDeltaRecorder(const HistogramFlattenerDeltaRecorder&) =
      delete;
  HistogramFlattenerDeltaRecorder& operator=(
      const HistogramFlattenerDeltaRecorder&) = delete;

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
};

// Exactly the same as FileMetricsProvider, but provides a way to "hook" into
// RecordSourcesChecked() and run a callback each time it is called so that it
// is easier to individually verify the sources being merged.
class TestFileMetricsProvider : public FileMetricsProvider {
 public:
  using FileMetricsProvider::FileMetricsProvider;

  TestFileMetricsProvider(const TestFileMetricsProvider&) = delete;
  TestFileMetricsProvider& operator=(const TestFileMetricsProvider&) = delete;

  ~TestFileMetricsProvider() override = default;

  // Sets the callback to run after RecordSourcesChecked() is called. Used to
  // individually verify the sources being merged.
  void SetSourcesCheckedCallback(base::RepeatingClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  // FileMetricsProvider:
  void RecordSourcesChecked(SourceInfoList* checked,
                            std::vector<size_t> samples_counts) override {
    if (!callback_.is_null()) {
      callback_.Run();
    }

    FileMetricsProvider::RecordSourcesChecked(checked, samples_counts);
  }

  // A callback to run after a call to RecordSourcesChecked().
  base::RepeatingClosure callback_;
};

class FileMetricsProviderTest : public testing::TestWithParam<bool> {
 public:
  FileMetricsProviderTest(const FileMetricsProviderTest&) = delete;
  FileMetricsProviderTest& operator=(const FileMetricsProviderTest&) = delete;

 protected:
  const size_t kSmallFileSize = 64 << 10;  // 64 KiB
  const size_t kLargeFileSize =  2 << 20;  //  2 MiB

  enum : int { kMaxCreateHistograms = 10 };

  FileMetricsProviderTest()
      : create_large_files_(GetParam()),
        statistics_recorder_(
            base::StatisticsRecorder::CreateTemporaryForTesting()),
        prefs_(new TestingPrefServiceSimple) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileMetricsProvider::RegisterSourcePrefs(prefs_->registry(), kMetricsName);
  }

  ~FileMetricsProviderTest() override {
    // Clear out any final remaining tasks.
    task_environment_.RunUntilIdle();
    DCHECK_EQ(0U, filter_actions_remaining_);
    // If a global histogram allocator exists at this point then it likely
    // acquired histograms that will continue to point to the released
    // memory and potentially cause use-after-free memory corruption.
    DCHECK(!base::GlobalHistogramAllocator::Get());
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  base::FilePath temp_dir() { return temp_dir_.GetPath(); }
  base::FilePath metrics_file() {
    return temp_dir_.GetPath().AppendASCII(kMetricsFilename);
  }

  TestFileMetricsProvider* provider() {
    if (!provider_)
      provider_ = std::make_unique<TestFileMetricsProvider>(prefs());
    return provider_.get();
  }

  void OnDidCreateMetricsLog() {
    provider()->OnDidCreateMetricsLog();
  }

  bool HasPreviousSessionData() { return provider()->HasPreviousSessionData(); }

  void MergeHistogramDeltas() {
    provider()->MergeHistogramDeltas(/*async=*/false,
                                     /*done_callback=*/base::DoNothing());
  }

  bool HasIndependentMetrics() { return provider()->HasIndependentMetrics(); }

  bool ProvideIndependentMetrics(
      ChromeUserMetricsExtension* uma_proto,
      base::HistogramSnapshotManager* snapshot_manager) {
    bool success = false;
    bool success_set = false;
    provider()->ProvideIndependentMetrics(
        base::DoNothing(),
        base::BindOnce(
            [](bool* success_ptr, bool* set_ptr, bool s) {
              *success_ptr = s;
              *set_ptr = true;
            },
            &success, &success_set),
        uma_proto, snapshot_manager);

    task_environment()->RunUntilIdle();
    CHECK(success_set);
    return success;
  }

  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) {
    provider()->RecordInitialHistogramSnapshots(snapshot_manager);
  }

  size_t GetSnapshotHistogramCount() {
    // Merge the data from the allocator into the StatisticsRecorder.
    MergeHistogramDeltas();

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
    provider()->ProvideIndependentMetrics(base::DoNothing(),
                                          base::BindOnce([](bool success) {}),
                                          &uma_proto, &snapshot_manager);

    task_environment()->RunUntilIdle();
    return flattener.GetRecordedDeltaHistogramNames().size();
  }

  void CreateGlobalHistograms(int histogram_count) {
    DCHECK_GT(kMaxCreateHistograms, histogram_count);

    // Create both sparse and normal histograms in the allocator. Make them
    // stability histograms to ensure that the histograms are snapshotted (in
    // the case of stability logs) or are put into independent logs. Histogram
    // names must be 2 characters (see HistogramFlattenerDeltaRecorder).
    created_histograms_[0] = base::SparseHistogram::FactoryGet(
        "h0", /*flags=*/base::HistogramBase::Flags::kUmaStabilityHistogramFlag);
    created_histograms_[0]->Add(0);
    for (int i = 1; i < histogram_count; ++i) {
      created_histograms_[i] = base::Histogram::FactoryGet(
          base::StringPrintf("h%d", i), 1, 100, 10,
          /*flags=*/base::HistogramBase::Flags::kUmaStabilityHistogramFlag);
      created_histograms_[i]->Add(i);
    }
  }

  void WriteMetricsFile(const base::FilePath& path,
                        base::PersistentHistogramAllocator* metrics) {
    base::File writer(path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    // Use DCHECK so the stack-trace will indicate where this was called.
    DCHECK(writer.IsValid()) << path;
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

  base::GlobalHistogramAllocator* CreateMetricsFileWithHistograms(
      const base::FilePath& file_path,
      base::Time write_time,
      int histogram_count,
      base::OnceCallback<void(base::PersistentHistogramAllocator*)> callback) {
    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        create_large_files_ ? kLargeFileSize : kSmallFileSize,
        0, kMetricsName);

    CreateGlobalHistograms(histogram_count);

    base::GlobalHistogramAllocator* histogram_allocator =
        base::GlobalHistogramAllocator::ReleaseForTesting();
    std::move(callback).Run(histogram_allocator);

    WriteMetricsFileAtTime(file_path, histogram_allocator, write_time);
    return histogram_allocator;
  }

  void CreateEmptyFile(const base::FilePath& file_path) {
    base::File empty(file_path,
                     base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  base::GlobalHistogramAllocator* CreateMetricsFileWithHistograms(
      int histogram_count) {
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
    params->filter = base::BindRepeating(
        &FileMetricsProviderTest::FilterSourcePath, base::Unretained(this));
  }

  const bool create_large_files_;

 private:
  FileMetricsProvider::FilterAction FilterSourcePath(
      const base::FilePath& path) {
    DCHECK_LT(0U, filter_actions_remaining_);
    --filter_actions_remaining_;
    return *filter_actions_++;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<TestFileMetricsProvider> provider_;
  base::HistogramBase* created_histograms_[kMaxCreateHistograms];

  raw_ptr<const FileMetricsProvider::FilterAction, AllowPtrArithmetic>
      filter_actions_ = nullptr;
  size_t filter_actions_remaining_ = 0;
};

// Run all test cases with both small and large files.
INSTANTIATE_TEST_SUITE_P(SmallAndLargeFiles,
                         FileMetricsProviderTest,
                         testing::Bool());

TEST_P(FileMetricsProviderTest, AccessMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));
  base::HistogramTester histogram_tester;

  base::Time metrics_time = base::Time::Now() - base::Minutes(5);
  base::GlobalHistogramAllocator* histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/true);
  histogram_tester.ExpectTotalCount(kMergedCountHistogramName,
                                    /*expected_count=*/0);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  histogram_tester.ExpectUniqueSample(kMergedCountHistogramName, /*sample=*/2,
                                      /*expected_bucket_count=*/1);
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  // File should have been deleted but recreate it to test behavior should
  // the file not be deletable by this process.
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator, metrics_time);

  // Second full run on the same file should produce nothing.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  histogram_tester.ExpectUniqueSample(kMergedCountHistogramName, /*sample=*/2,
                                      /*expected_bucket_count=*/1);
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Recreate the file to indicate that it is "new" and must be recorded.
  metrics_time = metrics_time + base::Minutes(1);
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator, metrics_time);

  // This run should again have "new" histograms.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  histogram_tester.ExpectUniqueSample(kMergedCountHistogramName, /*sample=*/2,
                                      /*expected_bucket_count=*/2);
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessTimeLimitedFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time metrics_time = base::Time::Now() - base::Hours(5);
  CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);

  // Register the file and allow the "checker" task to run.
  FileMetricsProvider::Params params(
      metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
      FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName);
  params.max_age = base::Hours(1);
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Attempt to access the file should return nothing.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, FilterDelaysFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time now_time = base::Time::Now();
  base::Time metrics_time = now_time - base::Minutes(5);
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
  SetFilterActions(&params, actions, std::size(actions));
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Processing the file should touch it but yield no results. File timestamp
  // accuracy is limited so compare the touched time to a couple seconds past.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
  ASSERT_TRUE(base::GetFileInfo(metrics_file(), &fileinfo));
  EXPECT_LT(metrics_time, fileinfo.last_modified);
  EXPECT_LE(now_time - base::Seconds(2), fileinfo.last_modified);

  // Second full run on the same file should process the file.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, FilterSkipsFile) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time now_time = base::Time::Now();
  base::Time metrics_time = now_time - base::Minutes(5);
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
  SetFilterActions(&params, actions, std::size(actions));
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Processing the file should delete it.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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
  base::Time base_time = base::Time::Now() - base::Minutes(10);

  // Create some files in an odd order. The files are "touched" back in time to
  // ensure that each file has a later timestamp on disk than the previous one.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII(".foo.pma"),
                         allocator, base_time);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("_bar.pma"),
                         allocator, base_time);
  // Histogram names must be 2 characters (see HistogramFlattenerDeltaRecorder).
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator, base_time + base::Minutes(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("c2.pma"),
                         allocator, base_time + base::Minutes(2));

  histogram = base::Histogram::FactoryGet("h3", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b3.pma"),
                         allocator, base_time + base::Minutes(3));

  histogram = base::Histogram::FactoryGet("h4", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("d4.pma"),
                         allocator, base_time + base::Minutes(4));

  base::TouchFile(metrics_files.GetPath().AppendASCII("b3.pma"),
                  base_time + base::Minutes(5), base_time + base::Minutes(5));

  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("baz"), allocator,
                         base_time + base::Minutes(6));

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_files.GetPath(),
          FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  std::vector<uint32_t> actual_order;
  provider()->SetSourcesCheckedCallback(base::BindLambdaForTesting(
      [&] { actual_order.push_back(GetSnapshotHistogramCount()); }));
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();

  // Files could come out in the order: a1, c2, d4, b3. They are recognizable by
  // the number of histograms contained within each. The "0" is the last merge
  // done, which detects that there are no more files to merge.
  EXPECT_THAT(actual_order, testing::ElementsAre(1, 2, 4, 3, 0));

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
  base::Time base_time = base::Time::Now() - base::Minutes(10);

  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  base::FilePath dir = metrics_files.GetPath();

  CreateMetricsFileWithHistograms(
      dir.AppendASCII("h1.pma"),
      base_time + base::Minutes(1), 1,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        allocator->memory_allocator()->SetMemoryState(
            base::PersistentMemoryAllocator::MEMORY_DELETED);
      }));

  CreateMetricsFileWithHistograms(
      dir.AppendASCII("h2.pma"),
      base_time + base::Minutes(2), 2,
      base::BindOnce(&WriteSystemProfileToAllocator));

  CreateMetricsFileWithHistograms(
      dir.AppendASCII("h3.pma"),
      base_time + base::Minutes(3), 3,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        allocator->memory_allocator()->SetMemoryState(
            base::PersistentMemoryAllocator::MEMORY_DELETED);
      }));

  CreateEmptyFile(dir.AppendASCII("h4.pma"));
  base::TouchFile(dir.AppendASCII("h4.pma"),
                  base_time + base::Minutes(4), base_time + base::Minutes(4));

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          dir,
          FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // No files yet.
  EXPECT_EQ(0U, GetIndependentHistogramCount());
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h1.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h2.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h4.pma")));

  // H1 should be skipped and H2 available.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(dir.AppendASCII("h1.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h2.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h4.pma")));

  // H2 should be read and the file deleted.
  EXPECT_EQ(2U, GetIndependentHistogramCount());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(dir.AppendASCII("h2.pma")));

  // Nothing else should be found but the last (valid but empty) file will
  // stick around to be processed later (should it get expanded).
  EXPECT_EQ(0U, GetIndependentHistogramCount());
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(dir.AppendASCII("h3.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h4.pma")));
}

TEST_P(FileMetricsProviderTest, AccessTimeLimitedDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::GlobalHistogramAllocator::CreateWithLocalMemory(64 << 10, 0,
                                                        kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create one old file and one new file. Histogram names must be 2 characters
  // (see HistogramFlattenerDeltaRecorder).
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator, base::Time::Now() - base::Hours(1));

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
  params.max_age = base::Minutes(30);
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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

  // Create one old file and one new file. Histogram names must be 2 characters
  // (see HistogramFlattenerDeltaRecorder).
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator, base::Time::Now() - base::Hours(1));

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
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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

  // Create one old file and one new file. Histogram names must be 2 characters
  // (see HistogramFlattenerDeltaRecorder).
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator, base::Time::Now() - base::Hours(1));

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
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Only b2, with 2 histograms, should be read.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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
  base::Time base_time = base::Time::Now() - base::Minutes(10);

  // Create some files in an odd order. The files are "touched" back in time to
  // ensure that each file has a later timestamp on disk than the previous one.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  // Histogram names must be 2 characters (see HistogramFlattenerDeltaRecorder).
  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("a1.pma"),
                         allocator, base_time + base::Minutes(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("c2.pma"),
                         allocator, base_time + base::Minutes(2));

  histogram = base::Histogram::FactoryGet("h3", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("b3.pma"),
                         allocator, base_time + base::Minutes(3));

  histogram = base::Histogram::FactoryGet("h4", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.GetPath().AppendASCII("d4.pma"),
                         allocator, base_time + base::Minutes(4));

  base::TouchFile(metrics_files.GetPath().AppendASCII("b3.pma"),
                  base_time + base::Minutes(5), base_time + base::Minutes(5));

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
  SetFilterActions(&params, actions, std::size(actions));
  provider()->RegisterSource(params, /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  std::vector<uint32_t> actual_order;
  provider()->SetSourcesCheckedCallback(base::BindLambdaForTesting(
      [&] { actual_order.push_back(GetSnapshotHistogramCount()); }));
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();

  // Files could come out in the order: a1, b3, c2. They are recognizable by the
  // number of histograms contained within each. The "0" is the last merge done,
  // which detects that there are no more files to merge.
  EXPECT_THAT(actual_order, testing::ElementsAre(1, 3, 2, 0));

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
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
          FileMetricsProvider::ASSOCIATE_CURRENT_RUN),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  ASSERT_TRUE(HasPreviousSessionData());
  task_environment()->RunUntilIdle();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // A run for normal histograms should produce nothing.
  CreateMetricsFileWithHistograms(2);
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsWithoutProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsWithProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce(&WriteSystemProfileToAllocator));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    RecordInitialHistogramSnapshots(&snapshot_manager);
    EXPECT_EQ(0U, flattener.GetRecordedDeltaHistogramNames().size());

    // A read of metrics with internal profiles should return one result, and
    // the independent log generated should have the embedded system profile.
    ChromeUserMetricsExtension uma_proto;
    EXPECT_TRUE(HasIndependentMetrics());
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
    ASSERT_TRUE(uma_proto.has_system_profile());
    ASSERT_EQ(1, uma_proto.system_profile().field_trial_size());
    EXPECT_EQ(123U, uma_proto.system_profile().field_trial(0).name_id());
    EXPECT_EQ(456U, uma_proto.system_profile().field_trial(0).group_id());
    EXPECT_EQ(789U, uma_proto.system_profile().session_hash());
    EXPECT_FALSE(HasIndependentMetrics());
    EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  }
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedFallbackMetricsWithoutProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
          kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  ASSERT_TRUE(HasPreviousSessionData());
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedFallbackMetricsWithProfile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce(&WriteSystemProfileToAllocator));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE_OR_PREVIOUS_RUN,
          kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  EXPECT_FALSE(HasPreviousSessionData());
  task_environment()->RunUntilIdle();
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
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessEmbeddedProfileMetricsFromDir) {
  const int file_count = 3;
  base::Time file_base_time = base::Time::Now();
  std::vector<base::FilePath> file_names;
  for (int i = 0; i < file_count; ++i) {
    CreateMetricsFileWithHistograms(
        metrics_file(), base::Time::Now(), 2,
        base::BindOnce(&WriteSystemProfileToAllocator));
    ASSERT_TRUE(PathExists(metrics_file()));
    char new_name[] = "hX";
    new_name[1] = '1' + i;
    base::FilePath file_name = temp_dir().AppendASCII(new_name).AddExtension(
        base::PersistentMemoryAllocator::kFileExtension);
    base::Time file_time = file_base_time - base::Minutes(file_count - i);
    base::TouchFile(metrics_file(), file_time, file_time);
    base::Move(metrics_file(), file_name);
    file_names.push_back(std::move(file_name));
  }

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          temp_dir(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE),
      /*metrics_reporting_enabled=*/true);

  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();

  // A read of metrics with internal profiles should return one result.
  HistogramFlattenerDeltaRecorder flattener;
  base::HistogramSnapshotManager snapshot_manager(&flattener);
  ChromeUserMetricsExtension uma_proto;
  for (int i = 0; i < file_count; ++i) {
    EXPECT_TRUE(HasIndependentMetrics()) << i;
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager)) << i;
    task_environment()->RunUntilIdle();
  }
  EXPECT_FALSE(HasIndependentMetrics());
  EXPECT_FALSE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));

  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  for (const auto& file_name : file_names)
    EXPECT_FALSE(base::PathExists(file_name));
}

TEST_P(FileMetricsProviderTest,
       RecordInitialHistogramSnapshotsStabilityHistograms) {
  // Create a metrics file with 2 non-stability histograms and 2 stability
  // histograms. Histogram names must be 2 characters (see
  // HistogramFlattenerDeltaRecorder).
  ASSERT_FALSE(PathExists(metrics_file()));
  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      create_large_files_ ? kLargeFileSize : kSmallFileSize, 0, kMetricsName);
  base::HistogramBase* h0 = base::SparseHistogram::FactoryGet(
      "h0", /*flags=*/base::HistogramBase::Flags::kUmaStabilityHistogramFlag);
  h0->Add(0);
  base::HistogramBase* h1 = base::SparseHistogram::FactoryGet(
      "h1", /*flags=*/base::HistogramBase::Flags::kUmaTargetedHistogramFlag);
  h1->Add(0);
  base::HistogramBase* h2 = base::Histogram::FactoryGet(
      "h2", 1, 100, 10,
      /*flags=*/base::HistogramBase::Flags::kUmaStabilityHistogramFlag);
  h2->Add(0);
  base::HistogramBase* h3 = base::Histogram::FactoryGet(
      "h3", 1, 100, 10,
      /*flags=*/base::HistogramBase::Flags::kUmaTargetedHistogramFlag);
  h3->Add(0);
  base::GlobalHistogramAllocator* histogram_allocator =
      base::GlobalHistogramAllocator::ReleaseForTesting();
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator,
                         base::Time::Now());
  ASSERT_TRUE(PathExists(metrics_file()));

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/true);
  ASSERT_TRUE(HasPreviousSessionData());
  task_environment()->RunUntilIdle();

  // Record embedded snapshots via snapshot-manager.
  HistogramFlattenerDeltaRecorder flattener;
  base::HistogramSnapshotManager snapshot_manager(&flattener);
  RecordInitialHistogramSnapshots(&snapshot_manager);

  // Verify that only the stability histograms were snapshotted.
  EXPECT_THAT(flattener.GetRecordedDeltaHistogramNames(),
              testing::ElementsAre("h0", "h2"));

  // The metrics file should eventually be deleted.
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, IndependentLogContainsUmaHistograms) {
  ASSERT_FALSE(PathExists(metrics_file()));
  // Create a metrics file with 2 UMA histograms and 2 non-UMA histograms.
  // Histogram names must be 2 characters (see HistogramFlattenerDeltaRecorder).
  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      create_large_files_ ? kLargeFileSize : kSmallFileSize, 0, kMetricsName);
  base::HistogramBase* h0 = base::SparseHistogram::FactoryGet(
      "h0", /*flags=*/base::HistogramBase::Flags::kUmaTargetedHistogramFlag);
  h0->Add(0);
  base::HistogramBase* h1 = base::SparseHistogram::FactoryGet(
      "h1", /*flags=*/base::HistogramBase::Flags::kNoFlags);
  h1->Add(0);
  base::HistogramBase* h2 = base::Histogram::FactoryGet(
      "h2", 1, 100, 10,
      /*flags=*/base::HistogramBase::Flags::kUmaStabilityHistogramFlag);
  h2->Add(0);
  base::HistogramBase* h3 = base::Histogram::FactoryGet(
      "h3", 1, 100, 10,
      /*flags=*/base::HistogramBase::Flags::kNoFlags);
  h3->Add(0);
  base::GlobalHistogramAllocator* histogram_allocator =
      base::GlobalHistogramAllocator::ReleaseForTesting();
  // Write a system profile so that an independent log can successfully be
  // created from the metrics file.
  WriteSystemProfileToAllocator(histogram_allocator);
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator,
                         base::Time::Now());
  ASSERT_TRUE(PathExists(metrics_file()));

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();

  // Verify that the independent log provided only contains UMA histograms (both
  // stability and non-stability).
  ChromeUserMetricsExtension uma_proto;
  HistogramFlattenerDeltaRecorder flattener;
  base::HistogramSnapshotManager snapshot_manager(&flattener);
  EXPECT_TRUE(HasIndependentMetrics());
  EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
  EXPECT_THAT(flattener.GetRecordedDeltaHistogramNames(),
              testing::ElementsAre("h0", "h2"));

  // The metrics file should eventually be deleted.
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

// Verifies that if the embedded system profile in the file does not contain
// a client UUID, the generated independent log's client ID is not overwritten.
TEST_P(FileMetricsProviderTest, EmbeddedProfileWithoutClientUuid) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce(&WriteSystemProfileToAllocator));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);

    // Since the embedded system profile has no client_uuid set (see
    // WriteSystemProfileToAllocator()), the client ID written in |uma_proto|
    // should be kept.
    ChromeUserMetricsExtension uma_proto;
    uma_proto.set_client_id(1);
    EXPECT_TRUE(HasIndependentMetrics());
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
    EXPECT_EQ(uma_proto.client_id(), 1U);
  }
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

// Verifies that if the embedded system profile in the file contains a client
// UUID, it is used as the generated independent log's client ID.
TEST_P(FileMetricsProviderTest, EmbeddedProfileWithClientUuid) {
  ASSERT_FALSE(PathExists(metrics_file()));
  static constexpr char kProfileClientUuid[] = "abc";
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now(), 2,
      base::BindOnce([](base::PersistentHistogramAllocator* allocator) {
        metrics::SystemProfileProto profile_proto;
        profile_proto.set_client_uuid(kProfileClientUuid);

        metrics::PersistentSystemProfile persistent_profile;
        persistent_profile.RegisterPersistentAllocator(
            allocator->memory_allocator());
        persistent_profile.SetSystemProfile(profile_proto, /*complete=*/true);
      }));

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/true);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  task_environment()->RunUntilIdle();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);

    // Since the embedded system profile contains a client_uuid, the client ID
    // in |uma_proto| should be overwritten.
    ChromeUserMetricsExtension uma_proto;
    uma_proto.set_client_id(1);
    EXPECT_TRUE(HasIndependentMetrics());
    EXPECT_TRUE(ProvideIndependentMetrics(&uma_proto, &snapshot_manager));
    EXPECT_NE(uma_proto.client_id(), 1U);
    EXPECT_EQ(uma_proto.client_id(), MetricsLog::Hash(kProfileClientUuid));
  }
  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, MetricsDisabledRegisterAtomicFile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now() - base::Minutes(10), 1,
      base::BindOnce(&WriteSystemProfileToAllocator));
  EXPECT_TRUE(base::PathExists(metrics_file()));

  EXPECT_TRUE(base::PathExists(metrics_file()));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
          FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/false);

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, MetricsDisabledRegisterAtomicDir) {
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  base::FilePath dir = metrics_files.GetPath();

  CreateMetricsFileWithHistograms(
      dir.AppendASCII("h1.pma"), base::Time::Now() - base::Minutes(10), 1,
      base::BindOnce(&WriteSystemProfileToAllocator));
  // Also create an empty file there to test the multiple-files in dir case.
  CreateEmptyFile(dir.AppendASCII("h2.pma"));

  EXPECT_TRUE(base::PathExists(dir));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h1.pma")));
  EXPECT_TRUE(base::PathExists(dir.AppendASCII("h2.pma")));
  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_files.GetPath(),
          FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
          FileMetricsProvider::ASSOCIATE_INTERNAL_PROFILE, kMetricsName),
      /*metrics_reporting_enabled=*/false);

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(base::PathExists(dir));
}

TEST_P(FileMetricsProviderTest, MetricsDisabledRegisterActiveFile) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(
      metrics_file(), base::Time::Now() - base::Minutes(10), 1,
      base::BindOnce(&WriteSystemProfileToAllocator));
  EXPECT_TRUE(base::PathExists(metrics_file()));

  provider()->RegisterSource(
      FileMetricsProvider::Params(
          metrics_file(), FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
          FileMetricsProvider::ASSOCIATE_CURRENT_RUN, kMetricsName),
      /*metrics_reporting_enabled=*/false);

  task_environment()->RunUntilIdle();
  // Active file should not be deleted.
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

}  // namespace metrics
