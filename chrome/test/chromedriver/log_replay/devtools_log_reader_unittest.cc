// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/chromedriver/log_replay/devtools_log_reader.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Log files to test the reader against
const char* const kTestDataPath[] = {"chrome", "test", "chromedriver",
                                     "log_replay", "test_data"};
const char kTestGetTitlePath[] = "testGetTitle_simple.log";
const char kOneEntryPath[] = "oneDevToolsEntry.log";
const char kTruncatedJSONPath[] = "truncatedJSON.log";
const char kReadableTimestampPathLinux[] = "testReadableTimestampLinux.log";
const char kReadableTimestampPathWin[] = "testReadableTimestampWindows.log";

base::FilePath GetLogFileFromLiteral(const char literal[]) {
  base::FilePath root_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_dir));
  for (int i = 0; i < 5; i++)
    root_dir = root_dir.AppendASCII(kTestDataPath[i]);
  base::FilePath result = root_dir.AppendASCII(literal);
  CHECK(base::PathExists(result));
  return result;
}
}  // namespace

TEST(DevToolsLogReaderTest, Basic) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::kHTTP);
  EXPECT_EQ(next->command_name, "http://localhost:38037/json/version");
  next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->payload, "{\n   \"string_key\": \"string_value\"\n}\n");
}

TEST(DevToolsLogReaderTest, ReadableTimeStampLinux) {
  base::FilePath path = GetLogFileFromLiteral(kReadableTimestampPathLinux);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::kHTTP);
  EXPECT_EQ(next->command_name, "http://localhost:38037/json/version");
  next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->payload, "{\n   \"string_key\": \"string_value\"\n}\n");
}

TEST(DevToolsLogReaderTest, ReadableTimeStampWindows) {
  base::FilePath path = GetLogFileFromLiteral(kReadableTimestampPathWin);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::kHTTP);
  EXPECT_EQ(next->command_name, "http://localhost:38037/json/version");
  next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->payload, "{\n   \"string_key\": \"string_value\"\n}\n");
}

TEST(DevToolsLogReaderTest, Multiple) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next;
  for (int i = 0; i < 3; i++)
    next = reader.GetNext(LogEntry::kHTTP);

  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->command_name, "http://localhost:38037/json");
  next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_EQ(next->payload,
            "[ {\n   \"string_key1\": \"string_value1\"\n}, {\n   "
            "\"string_key2\": \"string_value2\"\n} ]\n");
}

TEST(DevToolsLogReaderTest, EndOfFile) {
  base::FilePath path = GetLogFileFromLiteral(kOneEntryPath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next != nullptr);
  next = reader.GetNext(LogEntry::kHTTP);
  EXPECT_TRUE(next == nullptr);
}

TEST(DevToolsLogReaderTest, WebSocketBasic) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kWebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->protocol_type, LogEntry::kWebSocket);
  EXPECT_EQ(next->event_type, LogEntry::kRequest);
  EXPECT_EQ(next->command_name, "Log.enable");
  EXPECT_EQ(next->id, 1);
}

TEST(DevToolsLogReaderTest, WebSocketMultiple) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kWebSocket);
  next = reader.GetNext(LogEntry::kWebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->event_type, LogEntry::kRequest);
  EXPECT_EQ(next->command_name, "DOM.getDocument");
  EXPECT_EQ(next->id, 2);
}

TEST(DevToolsLogReaderTest, WebSocketPayload) {
  base::FilePath path = GetLogFileFromLiteral(kTestGetTitlePath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next;
  for (int i = 0; i < 3; i++)
    next = reader.GetNext(LogEntry::kWebSocket);
  EXPECT_TRUE(next != nullptr);
  EXPECT_EQ(next->command_name, "Target.setAutoAttach");
  EXPECT_EQ(next->id, 3);
  EXPECT_EQ(
      next->payload,
      "{\n   \"autoAttach\": true,\n   \"waitForDebuggerOnStart\": false\n}\n");
}

TEST(DevToolsLogReaderTest, TruncatedJSON) {
  base::FilePath path = GetLogFileFromLiteral(kTruncatedJSONPath);
  DevToolsLogReader reader(path);
  std::unique_ptr<LogEntry> next = reader.GetNext(LogEntry::kWebSocket);
  EXPECT_TRUE(next == nullptr);
}
