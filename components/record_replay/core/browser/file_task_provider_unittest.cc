// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/file_task_provider.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/record_replay/core/browser/task_definition_parsing_utils.h"
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

TEST(TaskDefinitionParsingUtilsTest, ParsesExtractionStrategy) {
  std::string json_content = R"({
    "url": "https://example.com/",
    "title": "Test Task",
    "description": "Task for dynamic extraction test.",
    "steps": [
      {
        "step_index": 0,
        "description": "Step with extraction parameters.",
        "url": "https://example.com/step0",
        "parameters": [
          {
            "key": "departure_date",
            "name": "Departure Date",
            "type": "string",
            "extraction_strategy": {
              "dom_css_selector": "trip-summary-section .date .title"
            }
          }
        ]
      }
    ]
  })";

  auto value = base::JSONReader::Read(json_content, 0);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());

  auto result = ParseTaskDefinition(value->GetDict());
  ASSERT_TRUE(result.has_value()) << result.error();

  const TaskDefinition& definition = result.value();
  EXPECT_EQ(definition.url(), "https://example.com/");
  EXPECT_EQ(definition.title(), "Test Task");
  EXPECT_EQ(definition.description(), "Task for dynamic extraction test.");

  ASSERT_EQ(definition.task_steps_size(), 1);
  const TaskStep& step = definition.task_steps(0);
  EXPECT_EQ(step.step_index(), 0);
  EXPECT_EQ(step.description(), "Step with extraction parameters.");
  EXPECT_EQ(step.url(), "https://example.com/step0");

  ASSERT_EQ(step.parameters_size(), 1);
  const TaskParameter& param = step.parameters(0);
  EXPECT_EQ(param.key(), "departure_date");
  EXPECT_EQ(param.name(), "Departure Date");
  EXPECT_EQ(param.type(), "string");

  ASSERT_TRUE(param.has_extraction_strategy());
  const ExtractionStrategy& strategy = param.extraction_strategy();
  ASSERT_TRUE(strategy.has_dom_css_selector());
  EXPECT_EQ(strategy.dom_css_selector(), "trip-summary-section .date .title");
}

}  // namespace
}  // namespace record_replay
