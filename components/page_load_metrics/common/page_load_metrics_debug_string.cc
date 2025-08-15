
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/page_load_metrics/common/page_load_metrics_debug_string.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"

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

std::string ElementsToStringList(const std::vector<std::string>& elements) {
  return base::StrCat({"[", base::JoinString(elements, ", "), "]"});
}
}  // namespace

std::string DebugString(const mojom::DocumentTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timing.dom_content_loaded_event_start) {
    entries.emplace_back(
        "dom_content_loaded_event_start",
        base::NumberToString(
            timing.dom_content_loaded_event_start->InMillisecondsF()));
  }
  if (timing.load_event_start) {
    entries.emplace_back(
        "load_event_start",
        base::NumberToString(timing.load_event_start->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::LcpResourceLoadTimings& timings) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timings.discovery_time) {
    entries.emplace_back(
        "discovery_time",
        base::NumberToString(timings.discovery_time->InMillisecondsF()));
  }
  if (timings.load_start) {
    entries.emplace_back(
        "load_start",
        base::NumberToString(timings.load_start->InMillisecondsF()));
  }
  if (timings.load_end) {
    entries.emplace_back(
        "load_end", base::NumberToString(timings.load_end->InMillisecondsF()));
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

std::string DebugString(const mojom::PaintTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timing.first_paint) {
    entries.emplace_back(
        "first_paint",
        base::NumberToString(timing.first_paint->InMillisecondsF()));
  }
  if (timing.first_image_paint) {
    entries.emplace_back(
        "first_image_paint",
        base::NumberToString(timing.first_image_paint->InMillisecondsF()));
  }
  if (timing.first_contentful_paint) {
    entries.emplace_back(
        "first_contentful_paint",
        base::NumberToString(timing.first_contentful_paint->InMillisecondsF()));
  }
  if (timing.first_meaningful_paint) {
    entries.emplace_back(
        "first_meaningful_paint",
        base::NumberToString(timing.first_meaningful_paint->InMillisecondsF()));
  }
  if (timing.largest_contentful_paint) {
    entries.emplace_back("largest_contentful_paint",
                         DebugString(*timing.largest_contentful_paint));
  }
  if (timing.experimental_largest_contentful_paint) {
    entries.emplace_back(
        "experimental_largest_contentful_paint",
        DebugString(*timing.experimental_largest_contentful_paint));
  }
  if (timing.first_eligible_to_paint) {
    entries.emplace_back(
        "first_eligible_to_paint",
        base::NumberToString(
            timing.first_eligible_to_paint->InMillisecondsF()));
  }
  if (timing.first_input_or_scroll_notified_timestamp) {
    entries.emplace_back(
        "first_input_or_scroll_notified_timestamp",
        base::NumberToString(timing.first_input_or_scroll_notified_timestamp
                                 ->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::ParseTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timing.parse_start) {
    entries.emplace_back(
        "parse_start",
        base::NumberToString(timing.parse_start->InMillisecondsF()));
  }
  if (timing.parse_stop) {
    entries.emplace_back(
        "parse_stop",
        base::NumberToString(timing.parse_stop->InMillisecondsF()));
  }
  if (timing.parse_blocked_on_script_load_duration) {
    entries.emplace_back(
        "parse_blocked_on_script_load_duration",
        base::NumberToString(
            timing.parse_blocked_on_script_load_duration->InMillisecondsF()));
  }
  if (timing.parse_blocked_on_script_load_from_document_write_duration) {
    entries.emplace_back(
        "parse_blocked_on_script_load_from_document_write_duration",
        base::NumberToString(
            timing.parse_blocked_on_script_load_from_document_write_duration
                ->InMillisecondsF()));
  }
  if (timing.parse_blocked_on_script_execution_duration) {
    entries.emplace_back(
        "parse_blocked_on_script_execution_duration",
        base::NumberToString(timing.parse_blocked_on_script_execution_duration
                                 ->InMillisecondsF()));
  }
  if (timing.parse_blocked_on_script_execution_from_document_write_duration) {
    entries.emplace_back(
        "parse_blocked_on_script_execution_from_document_write_duration",
        base::NumberToString(
            timing
                .parse_blocked_on_script_execution_from_document_write_duration
                ->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::InteractiveTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timing.first_input_delay) {
    entries.emplace_back(
        "first_input_delay",
        base::NumberToString(timing.first_input_delay->InMillisecondsF()));
  }
  if (timing.first_input_timestamp) {
    entries.emplace_back(
        "first_input_timestamp",
        base::NumberToString(timing.first_input_timestamp->InMillisecondsF()));
  }
  if (timing.first_scroll_delay) {
    entries.emplace_back(
        "first_scroll_delay",
        base::NumberToString(timing.first_scroll_delay->InMillisecondsF()));
  }
  if (timing.first_scroll_timestamp) {
    entries.emplace_back(
        "first_scroll_timestamp",
        base::NumberToString(timing.first_scroll_timestamp->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::DomainLookupTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  if (timing.domain_lookup_start) {
    entries.emplace_back(
        "domain_lookup_start",
        base::NumberToString(timing.domain_lookup_start->InMillisecondsF()));
  }
  if (timing.domain_lookup_end) {
    entries.emplace_back(
        "domain_lookup_end",
        base::NumberToString(timing.domain_lookup_end->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::PageLoadTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  entries.emplace_back(
      "navigation_start",
      base::NumberToString(
          timing.navigation_start.InMillisecondsSinceUnixEpoch()));
  if (timing.connect_start) {
    entries.emplace_back(
        "connect_start",
        base::NumberToString(timing.connect_start->InMillisecondsF()));
  }
  if (timing.connect_end) {
    entries.emplace_back(
        "connect_end",
        base::NumberToString(timing.connect_end->InMillisecondsF()));
  }
  if (timing.response_start) {
    entries.emplace_back(
        "response_start",
        base::NumberToString(timing.response_start->InMillisecondsF()));
  }
  if (timing.document_timing) {
    entries.emplace_back("document_timing",
                         DebugString(*timing.document_timing));
  }
  if (timing.interactive_timing) {
    entries.emplace_back("interactive_timing",
                         DebugString(*timing.interactive_timing));
  }
  if (timing.paint_timing) {
    entries.emplace_back("paint_timing", DebugString(*timing.paint_timing));
  }
  if (timing.parse_timing) {
    entries.emplace_back("parse_timing", DebugString(*timing.parse_timing));
  }
  if (timing.domain_lookup_timing) {
    entries.emplace_back("domain_lookup_timing",
                         DebugString(*timing.domain_lookup_timing));
  }
  if (!timing.back_forward_cache_timings.empty()) {
    std::vector<std::string> elements;
    for (const auto& e : timing.back_forward_cache_timings) {
      elements.push_back(DebugString(*e));
    }
    entries.emplace_back("back_forward_cache_timings",
                         ElementsToStringList(elements));
  }
  if (timing.activation_start) {
    entries.emplace_back(
        "activation_start",
        base::NumberToString(timing.activation_start->InMillisecondsF()));
  }
  if (timing.input_to_navigation_start) {
    entries.emplace_back(
        "input_to_navigation_start",
        base::NumberToString(
            timing.input_to_navigation_start->InMillisecondsF()));
  }
  if (timing.user_timing_mark_fully_loaded) {
    entries.emplace_back(
        "user_timing_mark_fully_loaded",
        base::NumberToString(
            timing.user_timing_mark_fully_loaded->InMillisecondsF()));
  }
  if (timing.user_timing_mark_fully_visible) {
    entries.emplace_back(
        "user_timing_mark_fully_visible",
        base::NumberToString(
            timing.user_timing_mark_fully_visible->InMillisecondsF()));
  }
  if (timing.user_timing_mark_interactive) {
    entries.emplace_back(
        "user_timing_mark_interactive",
        base::NumberToString(
            timing.user_timing_mark_interactive->InMillisecondsF()));
  }
  return EntriesToString(entries);
}

std::string DebugString(const mojom::BackForwardCacheTiming& timing) {
  std::vector<std::pair<std::string, std::string>> entries;
  entries.emplace_back(
      "first_paint_after_back_forward_cache_restore",
      base::NumberToString(timing.first_paint_after_back_forward_cache_restore
                               .InMillisecondsF()));
  if (!timing.request_animation_frames_after_back_forward_cache_restore
           .empty()) {
    std::vector<std::string> elements;
    for (const auto& e :
         timing.request_animation_frames_after_back_forward_cache_restore) {
      elements.push_back(base::NumberToString(e.InMillisecondsF()));
    }
    entries.emplace_back(
        "request_animation_frames_after_back_forward_cache_restore",
        ElementsToStringList(elements));
  }
  if (timing.first_input_delay_after_back_forward_cache_restore) {
    entries.emplace_back(
        "first_input_delay_after_back_forward_cache_restore",
        base::NumberToString(
            timing.first_input_delay_after_back_forward_cache_restore
                ->InMillisecondsF()));
  }
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
