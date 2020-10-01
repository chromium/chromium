// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_sorting_lsh_clusters_service.h"

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/federated_learning/floc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace federated_learning {

namespace {

class CopyingFileOutputStream
    : public google::protobuf::io::CopyingOutputStream {
 public:
  explicit CopyingFileOutputStream(base::File file) : file_(std::move(file)) {}

  CopyingFileOutputStream(const CopyingFileOutputStream&) = delete;
  CopyingFileOutputStream& operator=(const CopyingFileOutputStream&) = delete;

  ~CopyingFileOutputStream() override = default;

  // google::protobuf::io::CopyingOutputStream:
  bool Write(const void* buffer, int size) override {
    return file_.WriteAtCurrentPos(static_cast<const char*>(buffer), size) ==
           size;
  }

 private:
  base::File file_;
};

}  // namespace

class FlocSortingLshClustersServiceTest : public ::testing::Test {
 public:
  FlocSortingLshClustersServiceTest()
      : background_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()) {}

  FlocSortingLshClustersServiceTest(const FlocSortingLshClustersServiceTest&) =
      delete;
  FlocSortingLshClustersServiceTest& operator=(
      const FlocSortingLshClustersServiceTest&) = delete;

 protected:
  void SetUp() override {
    service_ = std::make_unique<FlocSortingLshClustersService>();
    service_->SetBackgroundTaskRunnerForTesting(background_task_runner_);
  }

  base::FilePath GetUniqueTemporaryPath() {
    CHECK(scoped_temp_dir_.IsValid() || scoped_temp_dir_.CreateUniqueTempDir());
    return scoped_temp_dir_.GetPath().AppendASCII(
        base::NumberToString(next_unique_file_suffix_++));
  }

  base::FilePath CreateTestSortingLshClustersFile(
      const std::vector<uint32_t>& sorting_lsh_clusters) {
    base::FilePath file_path = GetUniqueTemporaryPath();
    base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    CHECK(file.IsValid());

    CopyingFileOutputStream copying_stream(std::move(file));
    google::protobuf::io::CopyingOutputStreamAdaptor zero_copy_stream_adaptor(
        &copying_stream);

    google::protobuf::io::CodedOutputStream output_stream(
        &zero_copy_stream_adaptor);

    for (uint32_t next : sorting_lsh_clusters)
      output_stream.WriteVarint32(next);

    CHECK(!output_stream.HadError());

    return file_path;
  }

  base::FilePath InitializeSortingLshClustersFile(
      const std::vector<uint32_t>& sorting_lsh_clusters) {
    base::FilePath file_path =
        CreateTestSortingLshClustersFile(sorting_lsh_clusters);
    service()->OnSortingLshClustersFileReady(file_path);
    EXPECT_TRUE(sorting_lsh_clusters_file_path().has_value());
    return file_path;
  }

  FlocId MaxFlocId() { return FlocId((1ULL << kMaxNumberOfBitsInFloc) - 1); }

  FlocSortingLshClustersService* service() { return service_.get(); }

  const base::Optional<base::FilePath>& sorting_lsh_clusters_file_path() {
    return service()->sorting_lsh_clusters_file_path_;
  }

  FlocId ApplySortingLsh(const FlocId& floc_id) {
    FlocId result;

    base::RunLoop run_loop;
    auto cb = base::BindLambdaForTesting([&](FlocId floc_id) {
      result = floc_id;
      run_loop.Quit();
    });

    service()->ApplySortingLsh(floc_id, std::move(cb));
    background_task_runner_->RunPendingTasks();
    run_loop.Run();

    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix_ = 1;

  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_;

  std::unique_ptr<FlocSortingLshClustersService> service_;
};

TEST_F(FlocSortingLshClustersServiceTest, NoFilePath) {
  EXPECT_FALSE(sorting_lsh_clusters_file_path().has_value());
}

TEST_F(FlocSortingLshClustersServiceTest, EmptyList) {
  InitializeSortingLshClustersFile({});
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_0) {
  InitializeSortingLshClustersFile({0});

  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_1) {
  InitializeSortingLshClustersFile({1});

  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(2)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_0_0) {
  InitializeSortingLshClustersFile({0, 0});

  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(2)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_0_1) {
  InitializeSortingLshClustersFile({0, 1});

  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(2)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(3)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_1_0) {
  InitializeSortingLshClustersFile({1, 0});

  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(2)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(FlocId(3)));
  EXPECT_EQ(FlocId(), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_SingleCluster) {
  InitializeSortingLshClustersFile({kMaxNumberOfBitsInFloc});
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(12345)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest, List_TwoClustersEqualSize) {
  InitializeSortingLshClustersFile(
      {kMaxNumberOfBitsInFloc - 1, kMaxNumberOfBitsInFloc - 1});

  uint64_t middle_value = (1ULL << (kMaxNumberOfBitsInFloc - 1));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(1)));
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(middle_value - 1)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(middle_value)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(FlocId(middle_value + 1)));
  EXPECT_EQ(FlocId(1), ApplySortingLsh(MaxFlocId()));
}

TEST_F(FlocSortingLshClustersServiceTest,
       FileDeletedAfterSortingLshTaskScheduled) {
  base::FilePath file_path = InitializeSortingLshClustersFile({0});

  base::RunLoop run_loop;
  auto cb = base::BindLambdaForTesting([&](FlocId floc_id) {
    // Since the file has been deleted, expect an invalid floc id.
    EXPECT_EQ(FlocId(), floc_id);
    run_loop.Quit();
  });

  service()->ApplySortingLsh(FlocId(0), std::move(cb));
  base::DeleteFile(file_path);

  background_task_runner_->RunPendingTasks();
  run_loop.Run();
}

TEST_F(FlocSortingLshClustersServiceTest, MultipleUpdate_LatestOneUsed) {
  InitializeSortingLshClustersFile({});
  InitializeSortingLshClustersFile({0});
  EXPECT_EQ(FlocId(0), ApplySortingLsh(FlocId(0)));
}

}  // namespace federated_learning
