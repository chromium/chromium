
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/page_load_metrics/common/page_load_metrics_debug_string.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace page_load_metrics {
namespace {
std::string EntriesToString(
    const std::vector<std::pair<std::string, std::string>>& entries) {
  std::vector<std::string> entry_strings;
  for (const auto& [key, value] : entries) {
    entry_strings.push_back(base::StrCat({key, ": ", value}));
  }
  return base::StrCat({"{", base::JoinString(entry_strings, ", "), "}"});
}
}  // namespace

std::string DebugString(
    const mojom::LcpResourceLoadTimings& resource_load_timings) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (resource_load_timings.discovery_time) {
    entries.emplace_back(
        "discovery_time",
        base::NumberToString(
            resource_load_timings.discovery_time->InMillisecondsF()));
  }
  if (resource_load_timings.load_start) {
    entries.emplace_back(
        "load_start", base::NumberToString(
                          resource_load_timings.load_start->InMillisecondsF()));
  }
  if (resource_load_timings.load_end) {
    entries.emplace_back(
        "load_end", base::NumberToString(
                        resource_load_timings.load_end->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::LargestContentfulPaintTiming& lcp) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (lcp.largest_image_paint) {
    entries.emplace_back(
        "largest_image_paint",
        base::NumberToString(lcp.largest_image_paint->InMillisecondsF()));
  }
  entries.emplace_back("largest_image_paint_size",
                       base::NumberToString(lcp.largest_image_paint_size));
  if (lcp.largest_text_paint) {
    entries.emplace_back(
        "largest_text_paint",
        base::NumberToString(lcp.largest_text_paint->InMillisecondsF()));
  }
  entries.emplace_back("largest_text_paint_size",
                       base::NumberToString(lcp.largest_text_paint_size));
  if (lcp.resource_load_timings) {
    entries.emplace_back("resource_load_timings",
                         DebugString(*lcp.resource_load_timings));
  }
  entries.emplace_back("type", base::NumberToString(lcp.type));
  entries.emplace_back("image_bpp", base::NumberToString(lcp.image_bpp));
  entries.emplace_back("image_request_priority_valid",
                       base::NumberToString(lcp.image_request_priority_valid));
  entries.emplace_back(
      "image_request_priority_value",
      ::net::RequestPriorityToString(lcp.image_request_priority_value));
  return EntriesToString(entries);
}

std::string DebugString(
    const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  std::vector<std::pair<std::string, std::string>> entries;
  entries.emplace_back("count",
                       base::NumberToString(soft_navigation_metrics.count));
  entries.emplace_back(
      "start_time", base::NumberToString(
                        soft_navigation_metrics.start_time.InMillisecondsF()));
  entries.emplace_back(
      "navigation_id",
      base::NumberToString(soft_navigation_metrics.navigation_id));
  if (soft_navigation_metrics.largest_contentful_paint) {
    entries.emplace_back(
        "largest_contentful_paint",
        DebugString(*soft_navigation_metrics.largest_contentful_paint));
  }
  return EntriesToString(entries);
}
}  // namespace page_load_metrics
