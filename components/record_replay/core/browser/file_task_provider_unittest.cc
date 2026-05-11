// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/file_task_provider.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace record_replay {
namespace {

using testing::_;

class FileTaskProviderTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileTaskProviderTest, LoadValidJson) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("test.json");
  std::string json_content = R"([
    {
      "url": "https://coolwebsite.com/be-cool",
      "title": "Yes!",
      "instructions": "Be cool.",
      "anchored_message": "Be cool?"
    }
  ])";
  ASSERT_TRUE(base::WriteFile(file_path, json_content));

  FileTaskProvider provider(file_path);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(
      std::optional<TaskDiscoveryService::AutomationMetadata>)>
      callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce(
          [&run_loop](std::optional<TaskDiscoveryService::AutomationMetadata>
                          metadata) {
            ASSERT_TRUE(metadata.has_value());
            EXPECT_EQ(metadata->title, "Yes!");
            run_loop.Quit();
          });

  provider.ShouldOfferTask(GURL("https://coolwebsite.com/be-cool"),
                           callback.Get());
  run_loop.Run();
}

TEST_F(FileTaskProviderTest, HandleInvalidJson) {
  base::FilePath file_path = temp_dir_.GetPath().AppendASCII("invalid.json");
  ASSERT_TRUE(base::WriteFile(file_path, "invalid json"));

  FileTaskProvider provider(file_path);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(
      std::optional<TaskDiscoveryService::AutomationMetadata>)>
      callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce(
          [&run_loop](std::optional<TaskDiscoveryService::AutomationMetadata>
                          metadata) {
            EXPECT_FALSE(metadata.has_value());
            run_loop.Quit();
          });

  provider.ShouldOfferTask(GURL("https://example.com/booking"), callback.Get());
  run_loop.Run();
}

TEST_F(FileTaskProviderTest, HandleMissingFile) {
  base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("non_existent.json");

  FileTaskProvider provider(file_path);

  base::RunLoop run_loop;
  base::MockOnceCallback<void(
      std::optional<TaskDiscoveryService::AutomationMetadata>)>
      callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce(
          [&run_loop](std::optional<TaskDiscoveryService::AutomationMetadata>
                          metadata) {
            EXPECT_FALSE(metadata.has_value());
            run_loop.Quit();
          });

  provider.ShouldOfferTask(GURL("https://example.com/booking"), callback.Get());
  run_loop.Run();
}

}  // namespace
}  // namespace record_replay
