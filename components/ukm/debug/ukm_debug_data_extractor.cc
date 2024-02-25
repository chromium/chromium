// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/debug/ukm_debug_data_extractor.h"

#include <inttypes.h>

#include <map>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {
namespace debug {

namespace {

static const uint64_t BIT_FILTER_LAST32 = 0xffffffffULL;

struct SourceData {
  raw_ptr<UkmSource> source;
  std::vector<raw_ptr<mojom::UkmEntry, VectorExperimental>> entries;
};

std::string GetName(const ukm::builders::EntryDecoder& decoder, uint64_t hash) {
  const auto it = decoder.metric_map.find(hash);
  if (it == decoder.metric_map.end())
    return base::StringPrintf("Unknown %" PRIu64, hash);
  return it->second;
}

base::Value::Dict ConvertEntryToDict(const ukm::builders::DecodeMap& decode_map,
                                     const mojom::UkmEntry& entry) {
  base::Value::Dict entry_dict;

  const auto it = decode_map.find(entry.event_hash);
  if (it == decode_map.end()) {
    entry_dict.Set("name",
                   UkmDebugDataExtractor::UInt64AsPairOfInt(entry.event_hash));
  } else {
    entry_dict.Set("name", it->second.name);

    base::Value::List metrics_list;
    for (const auto& metric : entry.metrics) {
      base::Value::Dict metric_dict;
      metric_dict.Set("name", GetName(it->second, metric.first));
      metric_dict.Set("value",
                      UkmDebugDataExtractor::UInt64AsPairOfInt(metric.second));
      metrics_list.Append(std::move(metric_dict));
    }
    entry_dict.Set("metrics", std::move(metrics_list));
  }
  return entry_dict;
}

}  // namespace

UkmDebugDataExtractor::UkmDebugDataExtractor() = default;

UkmDebugDataExtractor::~UkmDebugDataExtractor() = default;

// static
base::Value UkmDebugDataExtractor::UInt64AsPairOfInt(uint64_t v) {
  // Convert int64_t to pair of int. Passing int64_t in base::Value is not
  // supported. The pair of int will be passed as a List.
  base::Value::List int_pair;
  int_pair.Append(static_cast<int>((v >> 32) & BIT_FILTER_LAST32));
  int_pair.Append(static_cast<int>(v & BIT_FILTER_LAST32));
  return base::Value(std::move(int_pair));
}

// static
base::Value UkmDebugDataExtractor::GetStructuredData(
    const UkmService* ukm_service) {
  if (!ukm_service)
    return {};

  base::Value::Dict ukm_data;

  ukm_data.Set("state", ukm_service->recording_enabled_);
  ukm_data.Set("msbb_state", ukm_service->recording_enabled(MSBB));
  ukm_data.Set("extension_state", ukm_service->recording_enabled(EXTENSIONS));
  ukm_data.Set("app_state", ukm_service->recording_enabled(APPS));
  ukm_data.Set("client_id",
               base::StringPrintf("%016" PRIx64, ukm_service->client_id_));
  ukm_data.Set("session_id", static_cast<int>(ukm_service->session_id_));

  ukm_data.Set("is_sampling_enabled",
               static_cast<bool>(ukm_service->IsSamplingConfigured()));

  std::map<SourceId, SourceData> source_data;
  for (const auto& kv : ukm_service->recordings_.sources) {
    source_data[kv.first].source = kv.second.get();
  }

  for (const auto& v : ukm_service->recordings_.entries) {
    source_data[v->source_id].entries.push_back(v.get());
  }

  base::Value::List sources_list;
  for (const auto& kv : source_data) {
    const auto* src = kv.second.source.get();

    base::Value::Dict source_dict;
    if (src) {
      source_dict.Set("id",
                      UkmDebugDataExtractor::UInt64AsPairOfInt(src->id()));
      source_dict.Set("url", base::Value(src->url().spec()));
      source_dict.Set("type", GetSourceIdTypeDebugString(src->id()));
    } else {
      source_dict.Set("id", UkmDebugDataExtractor::UInt64AsPairOfInt(kv.first));
      source_dict.Set("type", GetSourceIdTypeDebugString(kv.first));
    }

    base::Value::List entries_list;
    for (ukm::mojom::UkmEntry* entry : kv.second.entries) {
      entries_list.Append(ConvertEntryToDict(ukm_service->decode_map_, *entry));
    }

    source_dict.Set("events", std::move(entries_list));

    sources_list.Append(std::move(source_dict));
  }
  ukm_data.Set("sources", std::move(sources_list));
  return base::Value(std::move(ukm_data));
}

}  // namespace debug
}  // namespace ukm
