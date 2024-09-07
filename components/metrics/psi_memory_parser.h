// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PSI_MEMORY_PARSER_H_
#define COMPONENTS_METRICS_PSI_MEMORY_PARSER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"

namespace metrics {

// Items in internal are - as the name implies - NOT for outside consumption.
// Defined here to allow access to unit test.
namespace internal {

// Finds the bounds for a substring of |content| which is sandwiched between
// the given |prefix| and |suffix| indices. Search only considers
// the portion of the string starting from |search_start|.
// Returns false if the prefix and/or suffix are not found, true otherwise.
// |start| and |end| are output parameters populated with the indices
// for the middle string.
bool FindMiddleString(std::string_view content,
                      size_t search_start,
                      std::string_view prefix,
                      std::string_view suffix,
                      size_t* start,
                      size_t* end);

}  // namespace internal

// Values as logged in the histogram for memory pressure.
constexpr int kMemPressureMin = 1;  // As 0 is for underflow.
constexpr int kMemPressureExclusiveMax = 10000;
constexpr int kMemPressureHistogramBuckets = 100;

// Enumeration representing success and various failure modes for parsing PSI
// memory data. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class ParsePSIMemStatus {
  kSuccess,
  kReadFileFailed,
  kUnexpectedDataFormat,
  kInvalidMetricFormat,
  kParsePSIValueFailed,
  // Magic constant used by the histogram macros.
  kMaxValue = kParsePSIValueFailed,
};

// PSIMemoryParser has logic to parse results from /proc/memory/pressure
// in Linux, which can be used for memory pressure metrics.
class PSIMemoryParser {
 public:
  explicit PSIMemoryParser(uint32_t period);
  ~PSIMemoryParser();

  // Parses PSI memory pressure from  |content|, for the currently configured
  // metrics period (10, 60 or 300 seconds).
  // The some and full values are output to |metricSome| and |metricFull|,
  // respectively.
  // Returns status of the parse operation - ParsePSIMemStatus::kSuccess
  // or error code otherwise.
  ParsePSIMemStatus ParseMetrics(std::string_view content,
                                 int* metric_some,
                                 int* metric_full);

  uint32_t GetPeriod() const;
  void LogParseStatus(ParsePSIMemStatus stat);

  PSIMemoryParser(const PSIMemoryParser&) = delete;
  PSIMemoryParser& operator=(const PSIMemoryParser&) = delete;
  PSIMemoryParser() = delete;

 private:
  // Friend it so it can see private members for testing
  friend class PSIMemoryParserTest;
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, CustomInterval);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InvalidInterval);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InternalsA);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InternalsB);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InternalsC);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InternalsD);
  FRIEND_TEST_ALL_PREFIXES(PSIMemoryParserTest, InternalsE);

  ParsePSIMemStatus ParseMetricsInternal(const std::string& content,
                                         int* metric_some,
                                         int* metric_full);

  // Retrieves one metric value from |content|, for the currently configured
  // metrics category (10, 60 or 300 seconds).
  // Only considers the substring between |start| (inclusive) and |end|
  // (exclusive).
  // Returns the floating-point string representation converted into an integer
  // which has the value multiplied by 100 - (10.20 = 1020), for
  // histogram usage.
  int GetMetricValue(std::string_view content, size_t start, size_t end);

  std::string metric_prefix_;
  uint32_t period_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PSI_MEMORY_PARSER_H_
