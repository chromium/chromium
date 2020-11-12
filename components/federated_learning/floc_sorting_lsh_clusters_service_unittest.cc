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
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "components/federated_learning/floc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace federated_learning {

namespace {

const uint64_t kMaxSimHash = (1ULL << kMaxNumberOfBitsInFloc) - 1;

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

struct ApplySortingLshResult {
  base::Optional<uint64_t> final_hash;
  base::Version version;
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
      const std::vector<std::pair<uint32_t, bool>>& sorting_lsh_clusters) {
    base::FilePath file_path = GetUniqueTemporaryPath();
    base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    CHECK(file.IsValid());

    CopyingFileOutputStream copying_stream(std::move(file));
    google::protobuf::io::CopyingOutputStreamAdaptor zero_copy_stream_adaptor(
        &copying_stream);

    google::protobuf::io::CodedOutputStream output_stream(
        &zero_copy_stream_adaptor);

    for (const auto& p : sorting_lsh_clusters) {
      uint32_t next = p.first;
      bool is_blocked = p.second;
      if (is_blocked) {
        next |= kSortingLshBlockedMask;
      }
      output_stream.WriteVarint32(next);
    }

    CHECK(!output_stream.HadError());

    return file_path;
  }

  base::FilePath InitializeSortingLshClustersFile(
      const std::vector<std::pair<uint32_t, bool>>& sorting_lsh_clusters,
      const base::Version& version) {
    base::FilePath file_path =
        CreateTestSortingLshClustersFile(sorting_lsh_clusters);
    service()->OnSortingLshClustersFileReady(file_path, version);
    EXPECT_TRUE(sorting_lsh_clusters_file_path().has_value());
    return file_path;
  }

  FlocSortingLshClustersService* service() { return service_.get(); }

  base::Optional<base::FilePath> sorting_lsh_clusters_file_path() {
    if (!service()->first_file_ready_seen_)
      return base::nullopt;

    return service()->sorting_lsh_clusters_file_path_;
  }

  ApplySortingLshResult ApplySortingLsh(uint64_t sim_hash) {
    ApplySortingLshResult result;

    base::RunLoop run_loop;
    auto cb = base::BindLambdaForTesting(
        [&](base::Optional<uint64_t> final_hash, base::Version version) {
          result.final_hash = final_hash;
          result.version = version;
          run_loop.Quit();
        });

    service()->ApplySortingLsh(sim_hash, std::move(cb));
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
  InitializeSortingLshClustersFile({}, base::Version("2.3.4"));

  EXPECT_EQ(base::nullopt, ApplySortingLsh(0).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(1).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_0) {
  InitializeSortingLshClustersFile({{0, false}}, base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(base::Version("2.3.4"), ApplySortingLsh(0).version);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(1).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_0_Blocked) {
  InitializeSortingLshClustersFile({{0, true}}, base::Version("2.3.4"));

  EXPECT_EQ(base::nullopt, ApplySortingLsh(0).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(1).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_UnexpectedNumber) {
  InitializeSortingLshClustersFile({{1 << 8, false}}, base::Version("2.3.4"));

  EXPECT_EQ(base::nullopt, ApplySortingLsh(0).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(1).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_1) {
  InitializeSortingLshClustersFile({{1, false}}, base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(base::nullopt, ApplySortingLsh(2).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_0_0) {
  InitializeSortingLshClustersFile({{0, false}, {0, false}},
                                   base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(base::nullopt, ApplySortingLsh(2).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_0_1) {
  InitializeSortingLshClustersFile({{0, false}, {1, false}},
                                   base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(2).final_hash.value());
  EXPECT_EQ(base::nullopt, ApplySortingLsh(3).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_1_0) {
  InitializeSortingLshClustersFile({{1, false}, {0, false}},
                                   base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(2).final_hash.value());
  EXPECT_EQ(base::nullopt, ApplySortingLsh(3).final_hash);
  EXPECT_EQ(base::nullopt, ApplySortingLsh(kMaxSimHash).final_hash);
}

TEST_F(FlocSortingLshClustersServiceTest, List_SingleCluster) {
  InitializeSortingLshClustersFile({{kMaxNumberOfBitsInFloc, false}},
                                   base::Version("2.3.4"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(12345).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(kMaxSimHash).final_hash.value());
}

TEST_F(FlocSortingLshClustersServiceTest, List_TwoClustersEqualSize) {
  InitializeSortingLshClustersFile({{kMaxNumberOfBitsInFloc - 1, false},
                                    {kMaxNumberOfBitsInFloc - 1, false}},
                                   base::Version("2.3.4"));

  uint64_t middle_value = (1ULL << (kMaxNumberOfBitsInFloc - 1));
  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(1).final_hash.value());
  EXPECT_EQ(0u, ApplySortingLsh(middle_value - 1).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(middle_value).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(middle_value + 1).final_hash.value());
  EXPECT_EQ(1u, ApplySortingLsh(kMaxSimHash).final_hash.value());
}

TEST_F(FlocSortingLshClustersServiceTest,
       FileDeletedAfterSortingLshTaskScheduled) {
  base::FilePath file_path =
      InitializeSortingLshClustersFile({{0, false}}, base::Version("2.3.4"));

  base::RunLoop run_loop;
  auto cb = base::BindLambdaForTesting(
      [&](base::Optional<uint64_t> final_hash, base::Version version) {
        // Since the file has been deleted, expect an invalid final_hash.
        EXPECT_EQ(base::nullopt, final_hash);
        EXPECT_EQ(base::Version("2.3.4"), version);
        run_loop.Quit();
      });

  service()->ApplySortingLsh(/*sim_hash=*/0, std::move(cb));
  base::DeleteFile(file_path);

  background_task_runner_->RunPendingTasks();
  run_loop.Run();
}

TEST_F(FlocSortingLshClustersServiceTest, MultipleUpdate_LatestOneUsed) {
  InitializeSortingLshClustersFile({}, base::Version("2.3.4"));
  InitializeSortingLshClustersFile({{0, false}}, base::Version("6.7.8.9"));

  EXPECT_EQ(0u, ApplySortingLsh(0).final_hash.value());
  EXPECT_EQ(base::Version("6.7.8.9"), ApplySortingLsh(0).version);
}

}  // namespace federated_learning
