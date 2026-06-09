// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/logging/log_entry.h"

#include <optional>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {
namespace {
constexpr int64_t kTestNavigationId = 12345;

TEST(LogEntryTest, AllEventTypes) {
  struct {
    LogEventType event_type;
    std::string expected_string;
  } test_cases[] = {
      {LogEventType::kNavigationStarted, "NavigationStarted"},
      {LogEventType::kUrlEligibilityCheck, "UrlEligibilityCheck"},
      {LogEventType::kAnnotationExtractionStarted,
       "AnnotationExtractionStarted"},
      {LogEventType::kAnnotationsExtracted, "AnnotationsExtracted"},
      {LogEventType::kSuggestionGenerationStarted,
       "SuggestionGenerationStarted"},
      {LogEventType::kNoSupportedTasks, "NoSupportedTasks"},
      {LogEventType::kNoRelevantAnnotations, "NoRelevantAnnotations"},
      {LogEventType::kServerRequestSent, "ServerRequestSent"},
      {LogEventType::kServerResponseReceived, "ServerResponseReceived"},
      {LogEventType::kSuggestionGenerated, "SuggestionGenerated"},
      {LogEventType::kSuggestionSuppressed, "SuggestionSuppressed"},
      {LogEventType::kSuggestionCleared, "SuggestionCleared"},
      {LogEventType::kSuggestionShown, "SuggestionShown"},
      {LogEventType::kSuggestionAccepted, "SuggestionAccepted"},
      {LogEventType::kSuggestionDismissed, "SuggestionDismissed"},
      {LogEventType::kSuggestionIgnored, "SuggestionIgnored"},
      {LogEventType::kServerRequestFailed, "ServerRequestFailed"},
      {LogEventType::kServerResponseMalformed, "ServerResponseMalformed"},
  };

  for (const auto& test_case : test_cases) {
    LogEntry entry(kTestNavigationId, test_case.event_type, "");
    base::Value value_container = entry.ToValue();
    auto* event_type_str = value_container.GetDict().FindString("event_type");
    ASSERT_TRUE(event_type_str);
    EXPECT_EQ(*event_type_str, test_case.expected_string);
  }
}

TEST(LogEntryTest, InvalidEventTypeDCHECKs) {
  LogEntry entry(0, static_cast<LogEventType>(999), "");
  EXPECT_DCHECK_DEATH_WITH(entry.ToValue(), "NOTREACHED");
}

TEST(LogEntryTest, ToValueConversion) {
  LogEntry entry(kTestNavigationId, LogEventType::kUrlEligibilityCheck,
                 "example.com");
  entry.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(123456789LL);
  entry.details.Set("key", "value");

  base::Value value_container = entry.Clone().ToValue();
  const base::DictValue& dict = value_container.GetDict();

  std::optional<double> timestamp = dict.FindDouble("timestamp");
  ASSERT_TRUE(timestamp.has_value());
  EXPECT_DOUBLE_EQ(timestamp.value(), 123456.789);

  auto* navigation_id = dict.FindString("navigation_id");
  ASSERT_TRUE(navigation_id);
  EXPECT_EQ(*navigation_id, base::NumberToString(kTestNavigationId));

  auto* event_type = dict.FindString("event_type");
  ASSERT_TRUE(event_type);
  EXPECT_EQ(*event_type, "UrlEligibilityCheck");

  auto* source_etld = dict.FindString("source_etld_plus_1");
  ASSERT_TRUE(source_etld);
  EXPECT_EQ(*source_etld, "example.com");

  auto* details = dict.FindDict("details");
  ASSERT_TRUE(details);
  auto* key_val = details->FindString("key");
  ASSERT_TRUE(key_val);
  EXPECT_EQ(*key_val, "value");
}

}  // namespace
}  // namespace multistep_filter
