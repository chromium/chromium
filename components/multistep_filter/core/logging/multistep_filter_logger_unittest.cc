// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/multistep_filter_logger.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_log_router.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {

class TestLogRouter : public MultistepFilterLogRouter {
 public:
  TestLogRouter() = default;
  ~TestLogRouter() override = default;

  // MultistepFilterLogRouter:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool IsLoggingEnabled() const override { return is_logging_enabled_; }
  void SetIsLoggingEnabled(bool enabled) { is_logging_enabled_ = enabled; }

  std::vector<LogEntry> GetBufferedLogs() const override {
    std::vector<LogEntry> cloned_entries;
    cloned_entries.reserve(entries_.size());
    std::ranges::transform(entries_, std::back_inserter(cloned_entries),
                           &LogEntry::Clone);
    return cloned_entries;
  }

  void RouteLogMessage(LogEntry entry) override {
    entries_.push_back(std::move(entry));
  }

  base::RepeatingCallback<void(LogEntry)> GetLogCallback() override {
    return base::BindRepeating(&TestLogRouter::RouteLogMessage,
                               base::Unretained(this));
  }

  // Returns all intercepted log entries for test verification.
  const std::vector<LogEntry>& entries() const { return entries_; }

 private:
  bool is_logging_enabled_ = true;
  std::vector<LogEntry> entries_;
};

}  // namespace

TEST(MultistepFilterLoggerTest, ScopedLogMessage) {
  TestLogRouter router;
  {
    ScopedLogMessage(&router, "nav-1", LogEventType::kUiShown, "example.com");
  }

  ASSERT_EQ(router.entries().size(), 1u);
  const LogEntry& entry = router.entries().front();
  EXPECT_EQ(entry.navigation_id, "nav-1");
  EXPECT_EQ(entry.event_type, LogEventType::kUiShown);
  EXPECT_EQ(entry.source_etld_plus_1, "example.com");
}

TEST(MultistepFilterLoggerTest, ScopedLogMessageWithDetail) {
  TestLogRouter router;
  {
    ScopedLogMessage(&router, "nav-1", LogEventType::kUiShown, "example.com")
        .WithDetail("key1", "val1")
        .WithDetail("key2", 42);
  }

  ASSERT_EQ(router.entries().size(), 1u);
  const LogEntry& entry = router.entries().front();
  EXPECT_EQ(entry.navigation_id, "nav-1");

  auto* val1 = entry.details.FindString("key1");
  ASSERT_TRUE(val1);
  EXPECT_EQ(*val1, "val1");

  std::optional<int> val2 = entry.details.FindInt("key2");
  ASSERT_TRUE(val2.has_value());
  EXPECT_EQ(val2.value(), 42);
}

TEST(MultistepFilterLoggerTest, MacroLoggingEnabled) {
  TestLogRouter router;
  router.SetIsLoggingEnabled(true);

  MULTISTEP_FILTER_LOG(&router, "nav-2", LogEventType::kUiAccepted, "test.com")
      .WithDetail("detail_key", "detail_val");

  ASSERT_EQ(router.entries().size(), 1u);
  const LogEntry& entry = router.entries().front();
  EXPECT_EQ(entry.navigation_id, "nav-2");
  EXPECT_EQ(entry.event_type, LogEventType::kUiAccepted);
  EXPECT_EQ(entry.source_etld_plus_1, "test.com");

  auto* detail = entry.details.FindString("detail_key");
  ASSERT_TRUE(detail);
  EXPECT_EQ(*detail, "detail_val");
}

TEST(MultistepFilterLoggerTest, MacroLoggingDisabled) {
  TestLogRouter router;
  router.SetIsLoggingEnabled(false);

  MULTISTEP_FILTER_LOG(&router, "nav-3", LogEventType::kUiDismissed, "test.com")
      .WithDetail("detail_key", "detail_val");

  EXPECT_EQ(router.entries().size(), 0u);
}

}  // namespace multistep_filter
