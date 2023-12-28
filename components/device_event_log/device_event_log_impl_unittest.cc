// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_event_log/device_event_log_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_event_log {

namespace {

const size_t kDefaultMaxEvents = 100;
LogLevel kDefaultLevel = LOG_LEVEL_EVENT;
LogType kDefaultType = LOG_TYPE_NETWORK;
const char kFileName[] = "file";

// Calls GetAsString on the task thread and sets s_string_result. Make sure
// that task_runner_->RunUntilIdle() is called before using s_string_result.
std::string s_string_result;
void CallGetAsString(DeviceEventLogImpl* impl,
                     StringOrder order,
                     const std::string& format,
                     const std::string& types,
                     LogLevel max_level,
                     size_t max_events) {
  s_string_result =
      impl->GetAsString(order, format, types, max_level, max_events);
}

}  // namespace

class DeviceEventLogTest : public testing::Test {
 public:
  DeviceEventLogTest() : task_runner_(new base::TestSimpleTaskRunner()) {}

  DeviceEventLogTest(const DeviceEventLogTest&) = delete;
  DeviceEventLogTest& operator=(const DeviceEventLogTest&) = delete;

  void SetUp() override {
    impl_ =
        std::make_unique<DeviceEventLogImpl>(task_runner_, kDefaultMaxEvents);
  }

  void TearDown() override { impl_.reset(); }

 protected:
  std::string SkipTime(const std::string& input) {
    std::string output;
    for (const std::string& line : base::SplitString(
             input, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      size_t n = line.find(']');
      if (n != std::string::npos)
        output += "[time] " + line.substr(n + 2) + '\n';
      else
        output += line;
    }
    return output;
  }

  size_t CountLines(const std::string& input) {
    return base::ranges::count(input, '\n');
  }

  std::string GetAsString(StringOrder order,
                          const std::string& format,
                          const std::string& types,
                          LogLevel max_level,
                          size_t max_events) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CallGetAsString, impl_.get(), order, format,
                                  types, max_level, max_events));
    task_runner_->RunUntilIdle();
    return s_string_result;
  }

  std::string GetLogString(StringOrder order,
                           const std::string& format,
                           LogLevel max_level,
                           size_t max_events) {
    return GetAsString(order, format, "", max_level, max_events);
  }

  std::string GetOrderedString(StringOrder order, size_t max_events) {
    return GetAsString(order, "file", "", kDefaultLevel, max_events);
  }

  std::string GetLogStringForType(const std::string& types) {
    return GetAsString(OLDEST_FIRST, "type", types, kDefaultLevel, 0);
  }

  void AddNetworkEntry(const char* file,
                       int line,
                       LogLevel level,
                       const std::string& event) {
    impl_->AddEntry(file, line, kDefaultType, level, event);
    task_runner_->RunUntilIdle();
  }

  void AddTestEvent(LogLevel level, const std::string& event) {
    AddNetworkEntry(kFileName, 0, level, event);
  }

  void AddTestEventWithTimestamp(LogLevel level,
                                 const std::string& event,
                                 base::Time time) {
    impl_->AddEntryWithTimestampForTesting(kFileName, 0, kDefaultType, level,
                                           event, time);
    task_runner_->RunUntilIdle();
  }

  void AddEventType(LogType type, const std::string& event) {
    impl_->AddEntry(kFileName, 0, type, kDefaultLevel, event);
    task_runner_->RunUntilIdle();
  }

  size_t GetMaxEntries() const { return impl_->max_entries(); }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<DeviceEventLogImpl> impl_;
};

TEST_F(DeviceEventLogTest, TestNetworkEvents) {
  std::string output_none = GetOrderedString(OLDEST_FIRST, 0);
  EXPECT_EQ("No Log Entries.", output_none);

  LogLevel level = kDefaultLevel;
  static const char kFile1[] = "file1";
  static const char kFile2[] = "file2";
  static const char kFile3[] = "file3";
  AddNetworkEntry(kFile1, 1, level, "event1");
  AddNetworkEntry(kFile2, 2, level, "event2");
  AddNetworkEntry(kFile3, 3, level, "event3");
  AddNetworkEntry(kFile3, 3, level, "event3");

  const std::string expected_output_oldest_first(
      "file1:1 event1\n"
      "file2:2 event2\n"
      "file3:3 event3 (2)\n");
  std::string output_oldest_first = GetOrderedString(OLDEST_FIRST, 0);
  EXPECT_EQ(expected_output_oldest_first, output_oldest_first);

  const std::string expected_output_oldest_first_short(
      "file2:2 event2\n"
      "file3:3 event3 (2)\n");
  std::string output_oldest_first_short = GetOrderedString(OLDEST_FIRST, 2);
  EXPECT_EQ(expected_output_oldest_first_short, output_oldest_first_short);

  const std::string expected_output_newest_first(
      "file3:3 event3 (2)\n"
      "file2:2 event2\n"
      "file1:1 event1\n");
  std::string output_newest_first = GetOrderedString(NEWEST_FIRST, 0);
  EXPECT_EQ(expected_output_newest_first, output_newest_first);

  const std::string expected_output_newest_first_short(
      "file3:3 event3 (2)\n"
      "file2:2 event2\n");
  std::string output_newest_first_short = GetOrderedString(NEWEST_FIRST, 2);
  EXPECT_EQ(expected_output_newest_first_short, output_newest_first_short);
}

TEST_F(DeviceEventLogTest, TestMaxEntries) {
  const size_t max_entries = GetMaxEntries();
  const size_t entries_to_add = max_entries + 3;
  for (size_t i = 0; i < entries_to_add; ++i) {
    AddTestEvent(LOG_LEVEL_EVENT, base::StringPrintf("event_%" PRIuS, i));
  }
  std::string output = GetOrderedString(OLDEST_FIRST, 0);
  size_t output_lines = CountLines(output);
  EXPECT_EQ(max_entries, output_lines);
}

TEST_F(DeviceEventLogTest, TestStringFormat) {
  AddTestEvent(LOG_LEVEL_ERROR, "event0");
  EXPECT_EQ("file:0 event0\n",
            GetLogString(OLDEST_FIRST, "file", kDefaultLevel, 1));
  EXPECT_EQ("[time] event0\n",
            SkipTime(GetLogString(OLDEST_FIRST, "time", kDefaultLevel, 1)));
  EXPECT_EQ("event0\n", GetLogString(OLDEST_FIRST, "", kDefaultLevel, 1));
  EXPECT_EQ(
      "[time] file:0 event0\n",
      SkipTime(GetLogString(OLDEST_FIRST, "file,time", kDefaultLevel, 1)));

  AddTestEvent(LOG_LEVEL_DEBUG, "event1");

  AddTestEvent(kDefaultLevel, "event2");
  EXPECT_EQ("Network: file:0 event2\n",
            GetLogString(OLDEST_FIRST, "file,type", kDefaultLevel, 1));
}

TEST_F(DeviceEventLogTest, TestTimeFormat) {
  static constexpr base::Time::Exploded kTime = {.year = 2020,
                                                 .month = 1,
                                                 .day_of_month = 1,
                                                 .hour = 12,
                                                 .minute = 34,
                                                 .second = 56};
  base::Time time;
  ASSERT_TRUE(base::Time::FromLocalExploded(kTime, &time));

  AddTestEventWithTimestamp(LOG_LEVEL_EVENT, "event0", time);

  EXPECT_EQ("[12:34:56.000] event0\n",
            GetLogString(OLDEST_FIRST, "time", kDefaultLevel, 1));

#if BUILDFLAG(IS_POSIX)
  // External tools expect the "unixtime" format to match the format use in
  // /var/log/messages and other system logs.
  //
  // The "xxx" format specifier below converts to the timezone offset in
  // "+/-HH:MM" format.
  EXPECT_EQ(base::UnlocalizedTimeFormatWithPattern(
                time, "'2020-01-01T12:34:56.000000'xxx' event0\n'"),
            GetLogString(OLDEST_FIRST, "unixtime", kDefaultLevel, 1));
#endif
}

TEST_F(DeviceEventLogTest, TestLogLevel) {
  AddTestEvent(LOG_LEVEL_ERROR, "error1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");
  AddTestEvent(LOG_LEVEL_DEBUG, "debug6");

  std::string out;
  out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_ERROR, 0);
  EXPECT_EQ(3u, CountLines(out));
  out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0);
  EXPECT_EQ(5u, CountLines(out));
  out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0);
  EXPECT_EQ(6u, CountLines(out));

  // Test max_level. Get only the ERROR entries.
  out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_ERROR, 0);
  EXPECT_EQ("error1\nerror2\nerror4\n", out);
}

TEST_F(DeviceEventLogTest, TestMaxEvents) {
  AddTestEvent(LOG_LEVEL_EVENT, "event1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");

  // Oldest first
  EXPECT_EQ("error4\n", GetLogString(OLDEST_FIRST, "", LOG_LEVEL_ERROR, 1));

  EXPECT_EQ("error2\nerror4\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_ERROR, 2));

  EXPECT_EQ("event3\nerror4\nevent5\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 3));

  EXPECT_EQ("error2\nevent3\nerror4\nevent5\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 4));

  EXPECT_EQ("event1\nerror2\nevent3\nerror4\nevent5\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 5));

  // Newest first
  EXPECT_EQ("error4\n", GetLogString(NEWEST_FIRST, "", LOG_LEVEL_ERROR, 1));

  EXPECT_EQ("error4\nerror2\n",
            GetLogString(NEWEST_FIRST, "", LOG_LEVEL_ERROR, 2));

  EXPECT_EQ("event5\nerror4\nevent3\n",
            GetLogString(NEWEST_FIRST, "", LOG_LEVEL_EVENT, 3));

  EXPECT_EQ("event5\nerror4\nevent3\nerror2\n",
            GetLogString(NEWEST_FIRST, "", LOG_LEVEL_EVENT, 4));

  EXPECT_EQ("event5\nerror4\nevent3\nerror2\nevent1\n",
            GetLogString(NEWEST_FIRST, "", LOG_LEVEL_EVENT, 5));
}

TEST_F(DeviceEventLogTest, TestMaxErrors) {
  const int kMaxTestEntries = 4;
  impl_ = std::make_unique<DeviceEventLogImpl>(task_runner_, kMaxTestEntries);
  AddTestEvent(LOG_LEVEL_EVENT, "event1");
  AddTestEvent(LOG_LEVEL_ERROR, "error2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_ERROR, "error4");
  AddTestEvent(LOG_LEVEL_EVENT, "event5");
  AddTestEvent(LOG_LEVEL_EVENT, "event6");
  EXPECT_EQ("error2\nerror4\nevent5\nevent6\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_DEBUG, 0));
}

TEST_F(DeviceEventLogTest, TestType) {
  AddEventType(LOG_TYPE_NETWORK, "event1");
  AddEventType(LOG_TYPE_POWER, "event2");
  AddEventType(LOG_TYPE_NETWORK, "event3");
  AddEventType(LOG_TYPE_POWER, "event4");
  AddEventType(LOG_TYPE_NETWORK, "event5");
  AddEventType(LOG_TYPE_NETWORK, "event6");
  EXPECT_EQ(
      "Network: event1\nNetwork: event3\nNetwork: event5\nNetwork: event6\n",
      GetLogStringForType("network"));
  const std::string power_events("Power: event2\nPower: event4\n");
  EXPECT_EQ(power_events, GetLogStringForType("power"));
  EXPECT_EQ(power_events, GetLogStringForType("non-network"));
  const std::string all_events(
      "Network: event1\n"
      "Power: event2\n"
      "Network: event3\n"
      "Power: event4\n"
      "Network: event5\n"
      "Network: event6\n");
  EXPECT_EQ(all_events, GetLogStringForType("network,power"));
  EXPECT_EQ(all_events, GetLogStringForType(""));
}

TEST_F(DeviceEventLogTest, TestClear) {
  AddTestEvent(LOG_LEVEL_EVENT, "event1");
  AddTestEvent(LOG_LEVEL_EVENT, "event2");
  AddTestEvent(LOG_LEVEL_EVENT, "event3");
  AddTestEvent(LOG_LEVEL_EVENT, "event4");

  std::string out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0);
  EXPECT_EQ(4u, CountLines(out));

  impl_->Clear(base::Time(), base::Time::Max());

  out = GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0);
  EXPECT_EQ(0u, CountLines(out));
}

TEST_F(DeviceEventLogTest, TestClearRange) {
  AddTestEventWithTimestamp(LOG_LEVEL_EVENT, "event1",
                            base::Time() + base::Seconds(1));
  AddTestEventWithTimestamp(LOG_LEVEL_EVENT, "event2",
                            base::Time() + base::Seconds(2));
  AddTestEventWithTimestamp(LOG_LEVEL_EVENT, "event3",
                            base::Time() + base::Seconds(3));
  AddTestEventWithTimestamp(LOG_LEVEL_EVENT, "event4",
                            base::Time() + base::Seconds(4));

  EXPECT_EQ("event1\nevent2\nevent3\nevent4\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0));

  impl_->Clear(base::Time() + base::Seconds(2),
               base::Time() + base::Seconds(3));

  EXPECT_EQ("event1\nevent4\n",
            GetLogString(OLDEST_FIRST, "", LOG_LEVEL_EVENT, 0));
}

}  // namespace device_event_log
