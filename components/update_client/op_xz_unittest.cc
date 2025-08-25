// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_xz.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "components/update_client/test_utils.h"
#include "components/update_client/unzip/in_process_unzipper.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class XzOperationTest : public testing::Test {
 private:
  // `env_` must be constructed before sequence_checker_.
  base::test::TaskEnvironment env_;

 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath TempPath(base::FilePath basename) {
    return temp_dir_.GetPath().Append(basename);
  }

  base::FilePath CopyToTemp(const std::string& file_name) {
    base::FilePath dest = TempPath(base::FilePath().AppendUTF8(file_name));
    EXPECT_TRUE(base::CopyFile(GetTestFilePath(file_name.c_str()), dest));
    return dest;
  }

  base::RepeatingCallback<void(base::Value::Dict)> MakePingCallback() {
    return base::BindLambdaForTesting(
        [&](base::Value::Dict ping) { pings_.push_back(std::move(ping)); });
  }

  base::RepeatingCallback<void(update_client::ComponentState)>
  MakeStateCallback() {
    return base::BindRepeating([](update_client::ComponentState state) {
      ASSERT_EQ(state, update_client::ComponentState::kDecompressing);
    });
  }

  SEQUENCE_CHECKER(sequence_checker_);
  base::RunLoop loop_;
  std::vector<base::Value::Dict> pings_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(XzOperationTest, Success) {
  base::FilePath in_file = CopyToTemp("file1.xz");
  XzOperation(base::MakeRefCounted<InProcessUnzipperFactory>(
                  InProcessUnzipperFactory::SymlinkOption::DONT_PRESERVE)
                  ->Create(),
              MakePingCallback(), MakeStateCallback(), in_file,
              base::BindLambdaForTesting(
                  [&](base::expected<base::FilePath, CategorizedError> result) {
                    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                    ASSERT_TRUE(result.has_value());
                    EXPECT_TRUE(base::ContentsEqual(GetTestFilePath("file1"),
                                                    result.value()));
                  })
                  .Then(loop_.QuitClosure()));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(in_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), 60);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 1);
}

TEST_F(XzOperationTest, BadPatch) {
  base::FilePath in_file = CopyToTemp("file1");
  XzOperation(base::MakeRefCounted<InProcessUnzipperFactory>(
                  InProcessUnzipperFactory::SymlinkOption::DONT_PRESERVE)
                  ->Create(),
              MakePingCallback(), MakeStateCallback(), in_file,
              base::BindLambdaForTesting(
                  [&](base::expected<base::FilePath, CategorizedError> result) {
                    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
                    ASSERT_FALSE(result.has_value());
                  })
                  .Then(loop_.QuitClosure()));
  loop_.Run();
  EXPECT_FALSE(base::PathExists(in_file));
  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), 60);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
}

}  // namespace update_client
