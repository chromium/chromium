// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/federated_learning/floc_blocklist_service.h"

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

base::Version kDummyVersion = base::Version("1.0.0");

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

class FlocBlocklistServiceTest : public ::testing::Test {
 public:
  FlocBlocklistServiceTest()
      : background_task_runner_(
            base::MakeRefCounted<base::TestSimpleTaskRunner>()) {}

  FlocBlocklistServiceTest(const FlocBlocklistServiceTest&) = delete;
  FlocBlocklistServiceTest& operator=(const FlocBlocklistServiceTest&) = delete;

 protected:
  void SetUp() override {
    service_ = std::make_unique<FlocBlocklistService>();
    service_->SetBackgroundTaskRunnerForTesting(background_task_runner_);
  }

  base::FilePath GetUniqueTemporaryPath() {
    CHECK(scoped_temp_dir_.IsValid() || scoped_temp_dir_.CreateUniqueTempDir());
    return scoped_temp_dir_.GetPath().AppendASCII(
        base::NumberToString(next_unique_file_suffix_++));
  }

  base::FilePath CreateBlocklistFile(const std::vector<uint64_t>& blocklist) {
    base::FilePath file_path = GetUniqueTemporaryPath();
    base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
    CHECK(file.IsValid());

    CopyingFileOutputStream copying_stream(std::move(file));
    google::protobuf::io::CopyingOutputStreamAdaptor zero_copy_stream_adaptor(
        &copying_stream);

    google::protobuf::io::CodedOutputStream output_stream(
        &zero_copy_stream_adaptor);

    for (uint64_t next : blocklist)
      output_stream.WriteVarint64(next);

    CHECK(!output_stream.HadError());

    return file_path;
  }

  base::FilePath InitializeBlocklistFile(
      const std::vector<uint64_t>& blocklist) {
    base::FilePath file_path = CreateBlocklistFile(blocklist);
    service()->OnBlocklistFileReady(file_path, kDummyVersion);
    EXPECT_TRUE(blocklist_file_path().has_value());
    return file_path;
  }

  FlocId MaxFlocId() { return FlocId((1ULL << kMaxNumberOfBitsInFloc) - 1); }

  FlocBlocklistService* service() { return service_.get(); }

  base::Optional<base::FilePath> blocklist_file_path() {
    if (!service()->first_file_ready_seen_)
      return base::nullopt;

    return service()->blocklist_file_path_;
  }

  FlocId FilterByBlocklist(const FlocId& unfiltered_floc) {
    FlocId result;

    base::RunLoop run_loop;
    auto cb = base::BindLambdaForTesting([&](FlocId filtered_floc) {
      result = filtered_floc;
      run_loop.Quit();
    });

    service()->FilterByBlocklist(unfiltered_floc, kDummyVersion, std::move(cb));
    background_task_runner_->RunPendingTasks();
    run_loop.Run();

    return result;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  int next_unique_file_suffix_ = 1;

  scoped_refptr<base::TestSimpleTaskRunner> background_task_runner_;

  std::unique_ptr<FlocBlocklistService> service_;
};

TEST_F(FlocBlocklistServiceTest, NoFilePath) {
  EXPECT_FALSE(blocklist_file_path().has_value());
}

TEST_F(FlocBlocklistServiceTest, EmptyList) {
  InitializeBlocklistFile({});
  EXPECT_EQ(FlocId(0), FilterByBlocklist(FlocId(0)));
  EXPECT_EQ(FlocId(1), FilterByBlocklist(FlocId(1)));
  EXPECT_EQ(MaxFlocId(), FilterByBlocklist(MaxFlocId()));
}

TEST_F(FlocBlocklistServiceTest, List_0) {
  InitializeBlocklistFile({0});
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(0)));
  EXPECT_EQ(FlocId(1), FilterByBlocklist(FlocId(1)));
  EXPECT_EQ(MaxFlocId(), FilterByBlocklist(MaxFlocId()));
}

TEST_F(FlocBlocklistServiceTest, List_0_2) {
  InitializeBlocklistFile({0, 2});
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(0)));
  EXPECT_EQ(FlocId(1), FilterByBlocklist(FlocId(1)));
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(2)));
  EXPECT_EQ(FlocId(3), FilterByBlocklist(FlocId(3)));
  EXPECT_EQ(MaxFlocId(), FilterByBlocklist(MaxFlocId()));
}

TEST_F(FlocBlocklistServiceTest, List_1_Max) {
  InitializeBlocklistFile({1, MaxFlocId().ToUint64()});
  EXPECT_EQ(FlocId(0), FilterByBlocklist(FlocId(0)));
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(1)));
  EXPECT_EQ(FlocId(), FilterByBlocklist(MaxFlocId()));
}

TEST_F(FlocBlocklistServiceTest, List_MaxFlocPlus1) {
  InitializeBlocklistFile({(1ULL << kMaxNumberOfBitsInFloc)});
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(0)));
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(1)));
}

TEST_F(FlocBlocklistServiceTest, NonExistentBlocklist_Blocked) {
  base::FilePath file_path = GetUniqueTemporaryPath();
  service()->OnBlocklistFileReady(file_path, kDummyVersion);
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(3)));
}

TEST_F(FlocBlocklistServiceTest, MultipleUpdate_LatestOneLoaded) {
  InitializeBlocklistFile({500});
  InitializeBlocklistFile({600});
  EXPECT_EQ(FlocId(500), FilterByBlocklist(FlocId(500)));
  EXPECT_EQ(FlocId(), FilterByBlocklist(FlocId(600)));
}

}  // namespace federated_learning