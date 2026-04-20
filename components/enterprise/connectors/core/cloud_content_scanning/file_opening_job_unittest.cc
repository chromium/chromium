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
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
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

}  // namespace safe_browsing
