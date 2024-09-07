// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/psi_memory_parser.h"

#include <stddef.h>

#include <cinttypes>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/metrics/metrics_log_store.h"

namespace metrics {

namespace {

// Periods supported by standard Linux PSI metricvs.
constexpr uint32_t kMinCollectionInterval = 10;
constexpr uint32_t kMidCollectionInterval = 60;
constexpr uint32_t kMaxCollectionInterval = 300;

constexpr uint32_t kDefaultCollectionInterval = kMinCollectionInterval;

// Name of the histogram that represents the success and various failure modes
// for parsing PSI memory data.
const char kParsePSIMemoryHistogramName[] = "ChromeOS.CWP.ParsePSIMemory";

constexpr std::string_view kContentPrefixSome = "some";
constexpr std::string_view kContentPrefixFull = "full";
constexpr std::string_view kContentTerminator = " total=";
constexpr std::string_view kMetricTerminator = " ";

const char kMetricPrefixFormat[] = "avg%d=";

}  // namespace

PSIMemoryParser::PSIMemoryParser(uint32_t period)
    : period_(kDefaultCollectionInterval) {
  if (period == kMinCollectionInterval || period == kMidCollectionInterval ||
      period == kMaxCollectionInterval) {
    period_ = period;
  } else {
    LOG(WARNING) << "Ignoring invalid interval [" << period << "]";
  }

  metric_prefix_ = base::StringPrintf(kMetricPrefixFormat, period_);
}

PSIMemoryParser::~PSIMemoryParser() = default;

uint32_t PSIMemoryParser::GetPeriod() const {
  return period_;
}

int PSIMemoryParser::GetMetricValue(std::string_view content,
                                    size_t start,
                                    size_t end) {
  size_t value_start;
  size_t value_end;
  if (!internal::FindMiddleString(content, start, metric_prefix_,
                                  kMetricTerminator, &value_start,
                                  &value_end)) {
    return -1;
  }
  if (value_end > end) {
    return -1;  // Out of bounds of the search area.
  }

  double n;
  const std::string_view metric_value_text =
      content.substr(value_start, value_end - value_start);
  if (!base::StringToDouble(metric_value_text, &n)) {
    return -1;  // Unable to convert string to number
  }

  // Want to multiply by 100, but to avoid integer truncation,
  // do best-effort rounding.
  const int preround = static_cast<int>(n * 1000);
  return (preround + 5) / 10;
}

void PSIMemoryParser::LogParseStatus(ParsePSIMemStatus stat) {
  constexpr int statCeiling =
      static_cast<int>(ParsePSIMemStatus::kMaxValue) + 1;
  base::UmaHistogramExactLinear(kParsePSIMemoryHistogramName,
                                static_cast<int>(stat), statCeiling);
}

ParsePSIMemStatus PSIMemoryParser::ParseMetrics(std::string_view content,
                                                int* metric_some,
                                                int* metric_full) {
  size_t str_some_start;
  size_t str_some_end;
  size_t str_full_start;
  size_t str_full_end;

  // Example of content:
  //  some avg10=0.00 avg60=0.00 avg300=0.00 total=417963
  //  full avg10=0.00 avg60=0.00 avg300=0.00 total=205933
  // we will pick one of the columns depending on the colleciton period set

  DCHECK_NE(metric_some, nullptr);
  DCHECK_NE(metric_full, nullptr);

  if (!internal::FindMiddleString(content, 0, kContentPrefixSome,
                                  kContentTerminator, &str_some_start,
                                  &str_some_end)) {
    return ParsePSIMemStatus::kUnexpectedDataFormat;
  }

  if (!internal::FindMiddleString(content,
                                  str_some_end + kContentTerminator.length(),
                                  kContentPrefixFull, kContentTerminator,
                                  &str_full_start, &str_full_end)) {
    return ParsePSIMemStatus::kUnexpectedDataFormat;
  }

  int compute_some = GetMetricValue(content, str_some_start, str_some_end);
  if (compute_some < 0) {
    return ParsePSIMemStatus::kInvalidMetricFormat;
  }

  int compute_full = GetMetricValue(content, str_full_start, str_full_end);
  if (compute_full < 0) {
    return ParsePSIMemStatus::kInvalidMetricFormat;
  }

  *metric_some = compute_some;
  *metric_full = compute_full;

  return ParsePSIMemStatus::kSuccess;
}

namespace internal {

bool FindMiddleString(std::string_view content,
                      size_t search_start,
                      std::string_view prefix,
                      std::string_view suffix,
                      size_t* start,
                      size_t* end) {
  DCHECK_NE(start, nullptr);
  DCHECK_NE(end, nullptr);

  size_t compute_start = content.find(prefix, search_start);
  if (compute_start == std::string::npos) {
    return false;
  }
  compute_start += prefix.length();

  size_t compute_end = content.find(suffix, compute_start);
  if (compute_end == std::string::npos) {
    return false;
  }

  *start = compute_start;
  *end = compute_end;

  return true;
}

}  // namespace internal

}  // namespace metrics
