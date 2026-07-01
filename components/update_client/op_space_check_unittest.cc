// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_space_check.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace update_client {

class OpSpaceCheckTest : public testing::Test {
 public:
  OpSpaceCheckTest() = default;

 protected:
  base::RepeatingCallback<void(base::DictValue)> MakePingCallback() {
    return base::BindLambdaForTesting(
        [&](base::DictValue ping) { pings_.push_back(std::move(ping)); });
  }

  base::test::TaskEnvironment task_environment_;
  std::vector<base::DictValue> pings_;
};

TEST_F(OpSpaceCheckTest, EnoughSpaceForFullUpdate) {
  ProtocolParser::Pipeline pipeline;
  ProtocolParser::Operation download_op;
  download_op.type = "download";
  download_op.size = 5000;
  pipeline.operations.push_back(download_op);

  ProtocolParser::Operation crx_op;
  crx_op.type = "crx3";
  crx_op.size = 5000;
  pipeline.operations.push_back(crx_op);

  auto get_available_space = base::BindRepeating(
      [](const base::FilePath&) -> int64_t { return 15000; });

  base::FilePath path_in(FILE_PATH_LITERAL("some_path"));
  base::RunLoop run_loop;
  SpaceCheckOperation(
      pipeline, get_available_space, MakePingCallback(), path_in,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> outcome) {
            ASSERT_EQ(outcome, path_in);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(pings_.empty());
}

TEST_F(OpSpaceCheckTest, NotEnoughSpaceForFullUpdate) {
  ProtocolParser::Pipeline pipeline;
  ProtocolParser::Operation download_op;
  download_op.type = "download";
  download_op.size = 5000;
  pipeline.operations.push_back(download_op);

  ProtocolParser::Operation crx_op;
  crx_op.type = "crx3";
  crx_op.size = 5000;
  pipeline.operations.push_back(crx_op);

  auto get_available_space = base::BindRepeating(
      [](const base::FilePath&) -> int64_t { return 8000; });

  base::FilePath path_in(FILE_PATH_LITERAL("some_path"));
  base::RunLoop run_loop;
  SpaceCheckOperation(
      pipeline, get_available_space, MakePingCallback(), path_in,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> outcome) {
            ASSERT_FALSE(outcome.has_value());
            EXPECT_EQ(outcome.error().category, ErrorCategory::kDownload);
            EXPECT_EQ(outcome.error().code,
                      static_cast<int>(CrxDownloaderError::DISK_FULL));
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventDownload);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(CrxDownloaderError::DISK_FULL));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

TEST_F(OpSpaceCheckTest, EnoughSpaceForDiffUpdate) {
  ProtocolParser::Pipeline pipeline;
  ProtocolParser::Operation download_op;
  download_op.type = "download";
  download_op.size = 1000;
  pipeline.operations.push_back(download_op);

  ProtocolParser::Operation crx_op;
  crx_op.type = "crx3";
  crx_op.size = 5000;
  pipeline.operations.push_back(crx_op);

  auto get_available_space = base::BindRepeating(
      [](const base::FilePath&) -> int64_t { return 12000; });

  base::FilePath path_in(FILE_PATH_LITERAL("some_path"));
  base::RunLoop run_loop;
  SpaceCheckOperation(
      pipeline, get_available_space, MakePingCallback(), path_in,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> outcome) {
            ASSERT_EQ(outcome, path_in);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(pings_.empty());
}

TEST_F(OpSpaceCheckTest, NotEnoughSpaceForDiffUpdate) {
  ProtocolParser::Pipeline pipeline;
  ProtocolParser::Operation download_op;
  download_op.type = "download";
  download_op.size = 1000;
  pipeline.operations.push_back(download_op);

  ProtocolParser::Operation crx_op;
  crx_op.type = "crx3";
  crx_op.size = 5000;
  pipeline.operations.push_back(crx_op);

  auto get_available_space = base::BindRepeating(
      [](const base::FilePath&) -> int64_t { return 10000; });

  base::FilePath path_in(FILE_PATH_LITERAL("some_path"));
  base::RunLoop run_loop;
  SpaceCheckOperation(
      pipeline, get_available_space, MakePingCallback(), path_in,
      base::BindLambdaForTesting(
          [&](base::expected<base::FilePath, CategorizedError> outcome) {
            ASSERT_FALSE(outcome.has_value());
            EXPECT_EQ(outcome.error().category, ErrorCategory::kDownload);
            EXPECT_EQ(outcome.error().code,
                      static_cast<int>(CrxDownloaderError::DISK_FULL));
            run_loop.Quit();
          }));
  run_loop.Run();

  ASSERT_EQ(pings_.size(), 1u);
  EXPECT_EQ(pings_[0].FindInt("eventtype"), protocol_request::kEventDownload);
  EXPECT_EQ(pings_[0].FindInt("eventresult"), 0);
  EXPECT_EQ(pings_[0].FindInt("errorcode"),
            static_cast<int>(CrxDownloaderError::DISK_FULL));
  EXPECT_EQ(pings_[0].Find("extracode1"), nullptr);
}

}  // namespace update_client
