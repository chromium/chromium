// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/win/metrics_utils.h"

#include <cmath>
#include <optional>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/win/wmi.h"

namespace system_signals {

namespace {

// Returns a percentage number representing the error rate when given the number
// of successfully parsed items as `items_count`, and the number of items that
// were not successfully parsed as `errors_count`.
int CalculateErrorRate(size_t items_count, size_t errors_count) {
  auto total_count = items_count + errors_count;
  if (total_count == 0) {
    return 0;
  }

  return std::round(100.0 * errors_count / total_count);
}

template <typename T, typename U>
void LogResponse(std::string_view histogram_variant,
                 size_t items_count,
                 const std::optional<T>& query_error,
                 const std::vector<U>& parsing_errors) {
  static constexpr char kCollectionHistogramPrefix[] =
      "Enterprise.SystemSignals.Collection";
  if (query_error) {
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kCollectionHistogramPrefix, histogram_variant, ".QueryError"}),
        query_error.value());
    return;
  }

  static constexpr char kParsingError[] = ".ParsingError";
  if (!parsing_errors.empty()) {
    auto parsing_error_histogram = base::StrCat(
        {kCollectionHistogramPrefix, histogram_variant, kParsingError});
    for (auto error : parsing_errors) {
      base::UmaHistogramEnumeration(parsing_error_histogram, error);
    }
  }

  base::UmaHistogramPercentage(
      base::StrCat({kCollectionHistogramPrefix, histogram_variant,
                    kParsingError, ".Rate"}),
      CalculateErrorRate(items_count, parsing_errors.size()));
}

}  // namespace

void LogWscAvResponse(const device_signals::WscAvProductsResponse& response) {
  LogResponse(".WSC.AntiVirus", response.av_products.size(),
              response.query_error, response.parsing_errors);
}

void LogWmiHotfixResponse(const device_signals::WmiHotfixesResponse& response) {
  LogResponse(".WMI.Hotfixes", response.hotfixes.size(), response.query_error,
              response.parsing_errors);
}

}  // namespace system_signals
