// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/logging.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* const kAllWdLevels[] = {
  "ALL", "DEBUG", "INFO", "WARNING", "SEVERE", "OFF"
};

}

TEST(Logging, NameLevelConversionHappy) {
  // All names map to a valid enum value.
  for (int i = 0; static_cast<size_t>(i) < std::size(kAllWdLevels); ++i) {
    Log::Level level = static_cast<Log::Level>(-1);
    EXPECT_TRUE(WebDriverLog::NameToLevel(kAllWdLevels[i], &level));
    EXPECT_LE(Log::kAll, level);
    EXPECT_GE(Log::kOff, level);
  }
}

TEST(Logging, NameToLevelErrors) {
  Log::Level level = static_cast<Log::Level>(-1);
  EXPECT_FALSE(WebDriverLog::NameToLevel("A", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("B", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("H", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("R", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("T", &level));
  EXPECT_FALSE(WebDriverLog::NameToLevel("Z", &level));
  // The level variable was never modified.
  EXPECT_EQ(static_cast<Log::Level>(-1), level);
}

namespace {

void ValidateLogEntry(const base::Value::List& entries,
                      size_t index,
                      const std::string& expected_level,
                      const std::string& expected_message) {
  ASSERT_LT(index, entries.size());
  const base::Value& entry_value = entries[index];
  ASSERT_TRUE(entry_value.is_dict());
  const base::Value::Dict& entry = entry_value.GetDict();

  const std::string* level = entry.FindString("level");
  ASSERT_TRUE(level);
  EXPECT_EQ(*level, expected_level);

  const std::string* message = entry.FindString("message");
  ASSERT_TRUE(message);
  EXPECT_EQ(*message, expected_message);
  EXPECT_LT(0.0, *entry.FindDouble("timestamp"));
}

}  // namespace

TEST(WebDriverLog, Levels) {
  WebDriverLog log("type", Log::kInfo);
  log.AddEntry(Log::kInfo, std::string("info message"));
  log.AddEntry(Log::kError, "severe message");
  log.AddEntry(Log::kDebug, "debug message");  // Must not log

  base::Value::List entries = log.GetAndClearEntries();

  ASSERT_EQ(2u, entries.size());
  ValidateLogEntry(entries, 0u, "INFO", "info message");
  ValidateLogEntry(entries, 1u, "SEVERE", "severe message");
}

TEST(WebDriverLog, Off) {
  WebDriverLog log("type", Log::kOff);
  log.AddEntry(Log::kError, "severe message");  // Must not log
  log.AddEntry(Log::kDebug, "debug message");  // Must not log

  base::Value::List entries = log.GetAndClearEntries();
  EXPECT_TRUE(entries.empty());
}

TEST(WebDriverLog, All) {
  WebDriverLog log("type", Log::kAll);
  log.AddEntry(Log::kError, "severe message");
  log.AddEntry(Log::kDebug, "debug message");

  base::Value::List entries = log.GetAndClearEntries();

  ASSERT_EQ(2u, entries.size());
  ValidateLogEntry(entries, 0u, "SEVERE", "severe message");
  ValidateLogEntry(entries, 1u, "DEBUG", "debug message");
}

TEST(Logging, CreatePerformanceLog) {
  Capabilities capabilities;
  Session session("test");
  capabilities.logging_prefs["performance"] = Log::kInfo;
  capabilities.logging_prefs["browser"] = Log::kInfo;

  std::vector<std::unique_ptr<WebDriverLog>> logs;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  Status status = CreateLogs(capabilities, &session, &logs, &devtools_listeners,
                             &command_listeners);
  EXPECT_TRUE(status.IsOk());
  EXPECT_EQ(2u, devtools_listeners.size());
  EXPECT_EQ(1u, command_listeners.size());
  ASSERT_EQ(2u, logs.size());
  EXPECT_EQ("performance", logs[0]->type());
  EXPECT_EQ("browser", logs[1]->type());
}

TEST(Logging, IgnoreUnknownLogType) {
  Capabilities capabilities;
  Session session("test");
  capabilities.logging_prefs["gaga"] = Log::kInfo;

  std::vector<std::unique_ptr<WebDriverLog>> logs;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  Status status = CreateLogs(capabilities, &session, &logs, &devtools_listeners,
                             &command_listeners);
  EXPECT_TRUE(status.IsOk());
  EXPECT_EQ(1u, devtools_listeners.size());
  EXPECT_EQ(0u, command_listeners.size());
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ("browser", logs[0]->type());
}

TEST(Logging, DefaultLogs) {
  Capabilities capabilities;
  Session session("test");

  std::vector<std::unique_ptr<WebDriverLog>> logs;
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  Status status = CreateLogs(capabilities, &session, &logs, &devtools_listeners,
                             &command_listeners);
  EXPECT_TRUE(status.IsOk());
  EXPECT_EQ(1u, devtools_listeners.size());
  EXPECT_EQ(0u, command_listeners.size());
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ("browser", logs[0]->type());
}

TEST(Logging, GetFirstErrorMessage) {
  WebDriverLog log(WebDriverLog::kBrowserType, Log::kAll);
  std::string entry;

  entry = log.GetFirstErrorMessage();
  EXPECT_TRUE(entry.empty());

  log.AddEntry(Log::kInfo, "info message");
  log.AddEntry(Log::kError, "first error message");
  log.AddEntry(Log::kDebug, "debug message");
  log.AddEntry(Log::kError, "second error message");

  entry = log.GetFirstErrorMessage();
  EXPECT_EQ("first error message", entry);
}

TEST(Logging, OverflowLogs) {
  WebDriverLog log(WebDriverLog::kBrowserType, Log::kAll);
  for (size_t i = 0; i < internal::kMaxReturnedEntries; i++)
    log.AddEntry(Log::kInfo, base::StringPrintf("%" PRIuS, i));
  log.AddEntry(Log::kError, "the 1st error is in the 2nd batch");
  ASSERT_EQ("the 1st error is in the 2nd batch", log.GetFirstErrorMessage());
  base::Value::List entries = log.GetAndClearEntries();
  EXPECT_EQ(internal::kMaxReturnedEntries, entries.size());
  entries = log.GetAndClearEntries();
  EXPECT_EQ(1u, entries.size());
}
