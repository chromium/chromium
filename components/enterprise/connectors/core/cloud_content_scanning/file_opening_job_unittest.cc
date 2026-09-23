// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/file_opening_job.h"

#include <memory>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "build/build_config.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/file_analysis_request_base.h"
#include "components/enterprise/connectors/core/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
class MockFileAnalysisRequestBase : public FileAnalysisRequestBase {
 public:
  using FileAnalysisRequestBase::FileAnalysisRequestBase;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  MOCK_METHOD1(ProcessZipFile, void(Data));
  MOCK_METHOD1(ProcessRarFile, void(Data));
#endif
};

class AsyncHashingFileAnalysisRequest : public MockFileAnalysisRequestBase {
 public:
  using MockFileAnalysisRequestBase::MockFileAnalysisRequestBase;

  void OpenFile(const std::atomic<bool>* is_cancelled) override {
    start_hashing_.Wait();
    FileAnalysisRequestBase::OpenFile(is_cancelled);
  }

  base::TestWaitableEvent start_hashing_;
};
}  // namespace enterprise_connectors

namespace safe_browsing {
class FileOpeningJobTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void OnGotFileData(
      std::unique_ptr<enterprise_connectors::MockFileAnalysisRequestBase>
          request,
      enterprise_connectors::ScanRequestUploadResult result,
      enterprise_connectors::BinaryUploadRequest::Data data) {
    if (is_cancelled_test_) {
      EXPECT_TRUE(
          result == enterprise_connectors::ScanRequestUploadResult::kSuccess ||
          result ==
              enterprise_connectors::ScanRequestUploadResult::kUserCancelled);
    } else {
      EXPECT_EQ(enterprise_connectors::ScanRequestUploadResult::kSuccess,
                result);
    }
    EXPECT_TRUE(data.contents.empty());

    if (result == enterprise_connectors::ScanRequestUploadResult::kSuccess) {
      EXPECT_FALSE(data.mime_type.empty());
      EXPECT_EQ(3u, data.size);
      // printf "foo" | sha256sum |  tr '[:lower:]' '[:upper:]'
      EXPECT_EQ(
          "2C26B46B68FFC68FF99B453C1D30413413422D706483BFA0F98A5E886266E7AE",
          data.hash);
    } else {
      EXPECT_EQ(0u, data.size);
      EXPECT_EQ("", data.hash);
    }

    ++on_got_file_data_count_;
    if (on_got_file_data_count_ == quit_file_count_) {
      if (quit_closure_) {
        quit_closure_.Run();
      }
    }
  }

  std::vector<FileOpeningJob::FileOpeningTask> CreateFilesAndTasks(int num) {
    std::vector<FileOpeningJob::FileOpeningTask> tasks(num);

    for (int i = 0; i < num; ++i) {
      base::FilePath path = temp_dir_.GetPath().AppendASCII(
          base::StringPrintf("foo%d.txt", next_file_id_));
      ++next_file_id_;
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(base::byte_span_from_cstring("foo"));

      auto request =
          std::make_unique<enterprise_connectors::MockFileAnalysisRequestBase>(
              enterprise_connectors::AnalysisSettings(), path, path.BaseName(),
              /*mime_type*/ "",
              /*delay_opening_file*/ true, base::DoNothing(),
              base::NullCallback(),
              base::SingleThreadTaskRunner::GetCurrentDefault());
      enterprise_connectors::FileAnalysisRequestBase* request_raw =
          request.get();
      request_raw->GetRequestData(
          base::BindOnce(&FileOpeningJobTest::OnGotFileData,
                         weak_factory_.GetWeakPtr(), std::move(request)));
      tasks[i].request = request_raw;
    }

    return tasks;
  }

  // Waits for all file work to finish and for its replies to be delivered.
  void DrainFileWork() {
    // This is the barrier. The job is a ThreadPool task source, so the flush
    // blocks until that task source completes, meaning every
    // ProcessNextTask() (and so every OpenFile()) has returned, on whichever
    // worker thread it ran. base::test::RunUntil() cannot be used instead: it
    // only re-checks its condition when this thread goes idle, and a worker
    // finishing wakes nothing here.
    base::ThreadPoolInstance::Get()->FlushForTesting();

    // The flush blocks this thread, so the replies those workers posted back
    // are still sitting in this thread's queue. Run them. This part waits for
    // nothing; the workers are already done by now.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // ~FileOpeningJob() is what emits the cancellation metrics, so tests must
  // destroy the job before asserting on them.
  void DestroyJob(scoped_refptr<FileOpeningJob> job) {
    DrainFileWork();
    ASSERT_TRUE(job->HasOneRef()) << "job outlived its requests";
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;

  int next_file_id_ = 0;
  int on_got_file_data_count_ = 0;
  int quit_file_count_ = 0;
  bool is_cancelled_test_ = false;
  base::RepeatingClosure quit_closure_;

  base::WeakPtrFactory<FileOpeningJobTest> weak_factory_{this};
};

TEST_F(FileOpeningJobTest, SingleFile) {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 1;

  auto tasks = CreateFilesAndTasks(1);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  EXPECT_EQ(1, on_got_file_data_count_);
}

TEST_F(FileOpeningJobTest, MultiFiles) {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 100;

  auto tasks = CreateFilesAndTasks(100);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  EXPECT_EQ(100, on_got_file_data_count_);
}

TEST_F(FileOpeningJobTest, Cancel) {
  is_cancelled_test_ = true;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);

  auto tasks = CreateFilesAndTasks(50);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  job->Cancel();

  // Post a task to the ThreadPool to ensure that any pending tasks have
  // run, and wait for its completion instead of using RunUntilIdle().
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(FROM_HERE,
                                     {base::TaskPriority::BEST_EFFORT},
                                     base::DoNothing(), run_loop.QuitClosure());
  run_loop.Run();

  // The requests were cancelled. It is possible that some tasks were taken
  // before Cancel() took effect, but it shouldn't be all 50 tasks.
  EXPECT_LT(on_got_file_data_count_, 50);
}

TEST_F(FileOpeningJobTest, MaxThreadsFlag) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 500;

  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "10");
  EXPECT_EQ(10u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "0");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "foo");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());

  command_line->RemoveSwitch("wp-max-file-opening-threads");
  command_line->AppendSwitchASCII("wp-max-file-opening-threads", "-1");
  EXPECT_EQ(5u, FileOpeningJob::GetMaxFileOpeningThreads());
}

TEST_F(FileOpeningJobTest, CancelsWithoutHashOnDestroy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{enterprise_connectors::kEnableCancelUploadOnContentAnalysis, {}},
       {enterprise_connectors::kContentHashInFileUploadFinalCall, {}},
       {enterprise_connectors::kEnableNewUploadSizeLimit,
        {{"max_file_size_mb", "250"}}}},
      {});

  const size_t kLargeFileSize = 250 * 1024 * 1024 + 1;
  base::FilePath path = temp_dir_.GetPath().AppendASCII("large_file.txt");
  {
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    ASSERT_TRUE(file.SetLength(kLargeFileSize));
  }

  auto request =
      std::make_unique<enterprise_connectors::AsyncHashingFileAnalysisRequest>(
          enterprise_connectors::AnalysisSettings(), path, path.BaseName(),
          /*mime_type=*/"",
          /*delay_opening_file=*/true, base::DoNothing(), base::NullCallback(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), base::DoNothing(),
          /*is_obfuscated=*/false,
          /*force_sync_hash_computation=*/false);
  enterprise_connectors::AsyncHashingFileAnalysisRequest* request_raw =
      request.get();

  base::RunLoop request_data_cb_run_loop;
  base::RunLoop on_got_hash_run_loop;
  std::string computed_hash = "overwritten_sentinel";

  request_raw->GetRequestData(base::BindLambdaForTesting(
      [&](enterprise_connectors::ScanRequestUploadResult result,
          enterprise_connectors::BinaryUploadRequest::Data data) {
        EXPECT_EQ(
            result,
            enterprise_connectors::ScanRequestUploadResult::kFileTooLarge);
        EXPECT_EQ(data.hash, "");
        EXPECT_TRUE(request_raw->register_on_got_hash_callback_);
        request_raw->register_on_got_hash_callback_.Run(
            false, base::BindLambdaForTesting([&](std::string hash) {
              computed_hash = std::move(hash);
              on_got_hash_run_loop.Quit();
            }));
        request_data_cb_run_loop.Quit();
      }));

  std::vector<FileOpeningJob::FileOpeningTask> tasks(1);
  tasks[0].request = request_raw;
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));
  request_raw->start_hashing_.Signal();

  request_data_cb_run_loop.Run();

  // Destroy all references to the job. This must happen immediately, while
  // the hash is still pending, so no message loop may be run here.
  request->set_file_opening_job(nullptr);
  job.reset();

  on_got_hash_run_loop.Run();
  EXPECT_EQ(computed_hash, "");
}

TEST_F(FileOpeningJobTest, CancelMetricsRecordedForDrainedJob) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  // Let all the file work finish before cancelling.
  run_loop.Run();
  DrainFileWork();

  job->Cancel();
  DestroyJob(std::move(job));

  // Drained, so it must be classified as idle.
  histogram_tester.ExpectUniqueSample(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 0, 1);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration.InFlight", 0);
}

TEST_F(FileOpeningJobTest, CancelMetricsRecordedWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  DrainFileWork();

  job->Cancel();
  DestroyJob(std::move(job));

  // Both arms must report over the same population, so only sample presence
  // matters here, not the classification.
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 1);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", 1);
}

TEST_F(FileOpeningJobTest, NoCancelMetricsWithoutCancellation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  DrainFileWork();

  // Never cancelled, so nothing is recorded, not even on destruction.
  DestroyJob(std::move(job));

  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 0);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", 0);
}

TEST_F(FileOpeningJobTest, SignalCancelledStopsRemainingFiles) {
  is_cancelled_test_ = true;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);

  auto tasks = CreateFilesAndTasks(50);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  // The non-blocking signal alone must be enough to stop the rest of the
  // batch; no JobHandle::Cancel() is involved.
  job->SignalCancelled();

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(FROM_HERE,
                                     {base::TaskPriority::BEST_EFFORT},
                                     base::DoNothing(), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_LT(on_got_file_data_count_, 50);
}

TEST_F(FileOpeningJobTest, SignalCancelledRecordsCancelIntent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  DrainFileWork();

  // Signalling alone must produce the same metrics as Cancel().
  job->SignalCancelled();
  DestroyJob(std::move(job));

  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 1);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", 1);
}

TEST_F(FileOpeningJobTest, SignalCancelledRecordsMetricsWithFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  DrainFileWork();

  job->SignalCancelled();
  DestroyJob(std::move(job));

  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 1);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    1);
}

TEST_F(FileOpeningJobTest, SignalCancelledThenCancelRecordsOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      enterprise_connectors::kEnableCancelUploadOnContentAnalysis);
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  quit_file_count_ = 5;

  auto tasks = CreateFilesAndTasks(5);
  auto job = base::MakeRefCounted<FileOpeningJob>(std::move(tasks));

  run_loop.Run();
  DrainFileWork();

  // Real sequence: user's click signals, then teardown calls Cancel() twice.
  // Only one sample of each metric may be emitted.
  job->SignalCancelled();
  job->Cancel();
  job->Cancel();
  DestroyJob(std::move(job));

  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelledWhileInFlight", 1);
  histogram_tester.ExpectTotalCount("Enterprise.FileOpeningJob.CancelDuration",
                                    1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.FileOpeningJob.CancelBlockingDuration", 1);
}


}  // namespace safe_browsing
