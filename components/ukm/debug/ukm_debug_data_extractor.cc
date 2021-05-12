// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug/ukm_debug_data_extractor.h"

#include <inttypes.h>

#include <map>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {
namespace debug {

namespace {

static const uint64_t BIT_FILTER_LAST32 = 0xffffffffULL;

struct SourceData {
  UkmSource* source;
  std::vector<mojom::UkmEntry*> entries;
};

std::string GetName(const ukm::builders::EntryDecoder& decoder, uint64_t hash) {
  const auto it = decoder.metric_map.find(hash);
  if (it == decoder.metric_map.end())
    return base::StringPrintf("Unknown %" PRIu64, hash);
  return it->second;
}

base::Value ConvertEntryToValue(const ukm::builders::DecodeMap& decode_map,
                                const mojom::UkmEntry& entry) {
  base::DictionaryValue entry_value;

  const auto it = decode_map.find(entry.event_hash);
  if (it == decode_map.end()) {
    entry_value.SetKey(
        "name", UkmDebugDataExtractor::UInt64AsPairOfInt(entry.event_hash));
  } else {
    entry_value.SetKey("name", base::Value(it->second.name));

    base::ListValue metrics_list;
    for (const auto& metric : entry.metrics) {
      base::DictionaryValue metric_value;
      metric_value.SetKey("name",
                          base::Value(GetName(it->second, metric.first)));
      metric_value.SetKey(
          "value", UkmDebugDataExtractor::UInt64AsPairOfInt(metric.second));
      metrics_list.Append(std::move(metric_value));
    }
    entry_value.SetKey("metrics", std::move(metrics_list));
  }
  return std::move(entry_value);
}

}  // namespace

UkmDebugDataExtractor::UkmDebugDataExtractor() = default;

UkmDebugDataExtractor::~UkmDebugDataExtractor() = default;

// static
base::Value UkmDebugDataExtractor::UInt64AsPairOfInt(uint64_t v) {
  // Convert int64_t to pair of int. Passing int64_t in base::Value is not
  // supported. The pair of int will be passed as a ListValue.
  base::Value::ListStorage int_pair;
  int_pair.push_back(
      base::Value(static_cast<int>((v >> 32) & BIT_FILTER_LAST32)));
  int_pair.push_back(base::Value(static_cast<int>(v & BIT_FILTER_LAST32)));
  return base::Value(int_pair);
}

// static
base::Value UkmDebugDataExtractor::GetStructuredData(
    const UkmService* ukm_service) {
  if (!ukm_service)
    return {};

  base::DictionaryValue ukm_data;

  ukm_data.SetKey("state", base::Value(ukm_service->recording_enabled_));
  ukm_data.SetKey("client_id", base::Value(base::StringPrintf(
                                   "%016" PRIx64, ukm_service->client_id_)));
  ukm_data.SetKey("session_id",
                  base::Value(static_cast<int>(ukm_service->session_id_)));

  ukm_data.SetKey(
      "is_sampling_enabled",
      base::Value(static_cast<bool>(ukm_service->IsSamplingEnabled())));

  std::map<SourceId, SourceData> source_data;
  for (const auto& kv : ukm_service->recordings_.sources) {
    source_data[kv.first].source = kv.second.get();
  }

  for (const auto& v : ukm_service->recordings_.entries) {
    source_data[v->source_id].entries.push_back(v.get());
  }

  base::ListValue sources_list;
  for (const auto& kv : source_data) {
    const auto* src = kv.second.source;

    base::DictionaryValue source_value;
    if (src) {
      source_value.SetKey("id",
                          UkmDebugDataExtractor::UInt64AsPairOfInt(src->id()));
      source_value.SetKey("url", base::Value(src->url().spec()));
    } else {
      source_value.SetKey("id",
                          UkmDebugDataExtractor::UInt64AsPairOfInt(kv.first));
    }

    base::ListValue entries_list;
    for (auto* entry : kv.second.entries) {
      entries_list.Append(
          ConvertEntryToValue(ukm_service->decode_map_, *entry));
    }

    source_value.SetKey("entries", std::move(entries_list));

    sources_list.Append(std::move(source_value));
  }
  ukm_data.SetKey("sources", std::move(sources_list));
  return std::move(ukm_data);
}

}  // namespace debug
}  // namespace ukm
