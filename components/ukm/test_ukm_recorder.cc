// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"

#include <iterator>
#include <string_view>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/ranges/algorithm.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

// Merge the data from |in| to |out|.
void MergeEntry(const mojom::UkmEntry* in, mojom::UkmEntry* out) {
  if (out->event_hash) {
    EXPECT_EQ(out->source_id, in->source_id);
    EXPECT_EQ(out->event_hash, in->event_hash);
  } else {
    out->event_hash = in->event_hash;
    out->source_id = in->source_id;
  }
  for (const auto& metric : in->metrics) {
    out->metrics.emplace(metric);
  }
}

}  // namespace

TestUkmRecorder::TestUkmRecorder() {
  UpdateRecording(UkmConsentState::All());
  InitDecodeMap();
  SetSamplingForTesting(1);  // 1-in-1 == unsampled
}

TestUkmRecorder::~TestUkmRecorder() = default;

void TestUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  const bool should_run_callback =
      on_add_entry_ && entry && entry_hash_to_wait_for_ == entry->event_hash;
  UkmRecorderImpl::AddEntry(std::move(entry));
  if (should_run_callback)
    on_add_entry_.Run();
}

const UkmSource* TestUkmRecorder::GetSourceForSourceId(
    SourceId source_id) const {
  const UkmSource* source = nullptr;
  for (const auto& kv : sources()) {
    if (kv.second->id() == source_id) {
      DCHECK_EQ(nullptr, source);
      source = kv.second.get();
    }
  }
  return source;
}

const ukm::mojom::UkmEntry* TestUkmRecorder::GetDocumentCreatedEntryForSourceId(
    ukm::SourceId source_id) const {
  auto entries = GetEntriesByName(ukm::builders::DocumentCreated::kEntryName);
  for (const ukm::mojom::UkmEntry* entry : entries) {
    if (entry->source_id == source_id)
      return entry;
  }
  return nullptr;
}

void TestUkmRecorder::SetOnAddEntryCallback(
    std::string_view entry_name,
    base::RepeatingClosure on_add_entry) {
  on_add_entry_ = std::move(on_add_entry);
  entry_hash_to_wait_for_ = base::HashMetricName(entry_name);
}

std::vector<raw_ptr<const mojom::UkmEntry, VectorExperimental>>
TestUkmRecorder::GetEntriesByName(std::string_view entry_name) const {
  uint64_t hash = base::HashMetricName(entry_name);
  std::vector<raw_ptr<const mojom::UkmEntry, VectorExperimental>> result;
  for (const auto& it : entries()) {
    if (it->event_hash == hash)
      result.push_back(it.get());
  }
  return result;
}

std::map<ukm::SourceId, mojom::UkmEntryPtr>
TestUkmRecorder::GetMergedEntriesByName(std::string_view entry_name) const {
  uint64_t hash = base::HashMetricName(entry_name);
  std::map<ukm::SourceId, mojom::UkmEntryPtr> result;
  for (const auto& it : entries()) {
    if (it->event_hash != hash)
      continue;
    mojom::UkmEntryPtr& entry_ptr = result[it->source_id];
    if (!entry_ptr)
      entry_ptr = mojom::UkmEntry::New();
    MergeEntry(it.get(), entry_ptr.get());
  }
  return result;
}

void TestUkmRecorder::ExpectEntrySourceHasUrl(const mojom::UkmEntry* entry,
                                              const GURL& url) const {
  const UkmSource* src = GetSourceForSourceId(entry->source_id);
  if (src == nullptr) {
    FAIL() << "Entry source id has no associated Source.";
  }
  EXPECT_EQ(src->url(), url);
}

// static
bool TestUkmRecorder::EntryHasMetric(const mojom::UkmEntry* entry,
                                     std::string_view metric_name) {
  return GetEntryMetric(entry, metric_name) != nullptr;
}

// static
const int64_t* TestUkmRecorder::GetEntryMetric(const mojom::UkmEntry* entry,
                                               std::string_view metric_name) {
  uint64_t hash = base::HashMetricName(metric_name);
  const auto it = entry->metrics.find(hash);
  if (it != entry->metrics.end())
    return &it->second;
  return nullptr;
}

// static
void TestUkmRecorder::ExpectEntryMetric(const mojom::UkmEntry* entry,
                                        std::string_view metric_name,
                                        int64_t expected_value) {
  const int64_t* metric = GetEntryMetric(entry, metric_name);
  if (metric == nullptr) {
    FAIL() << "Failed to find metric for event: " << metric_name;
  }
  EXPECT_EQ(expected_value, *metric) << " for metric:" << metric_name;
}

TestAutoSetUkmRecorder::TestAutoSetUkmRecorder() {
  DelegatingUkmRecorder::Get()->AddDelegate(self_ptr_factory_.GetWeakPtr());
}

TestAutoSetUkmRecorder::~TestAutoSetUkmRecorder() {
  DelegatingUkmRecorder::Get()->RemoveDelegate(this);
}

std::vector<TestUkmRecorder::HumanReadableUkmMetrics>
TestUkmRecorder::GetMetrics(
    std::string entry_name,
    const std::vector<std::string>& metric_names) const {
  std::vector<TestUkmRecorder::HumanReadableUkmMetrics> result;
  for (const auto& entry : GetEntries(entry_name, metric_names)) {
    result.push_back(entry.metrics);
  }
  return result;
}

std::vector<int64_t> TestUkmRecorder::GetMetricsEntryValues(
    const std::string& entry_name,
    const std::string& metric_name) const {
  const auto metric_entries = GetMetrics(entry_name, {metric_name});
  std::vector<int64_t> metric_values;
  for (const auto& entry : metric_entries) {
    auto it = entry.find(metric_name);
    if (it != entry.end()) {
      metric_values.push_back(it->second);
    }
  }
  return metric_values;
}

std::vector<TestUkmRecorder::HumanReadableUkmEntry> TestUkmRecorder::GetEntries(
    std::string entry_name,
    const std::vector<std::string>& metric_names) const {
  std::vector<TestUkmRecorder::HumanReadableUkmEntry> results;
  for (const ukm::mojom::UkmEntry* entry : GetEntriesByName(entry_name)) {
    HumanReadableUkmEntry result;
    result.source_id = entry->source_id;
    for (const std::string& metric_name : metric_names) {
      const int64_t* metric_value =
          ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
      if (metric_value)
        result.metrics[metric_name] = *metric_value;
    }
    results.push_back(std::move(result));
  }
  return results;
}

std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics>
TestUkmRecorder::FilteredHumanReadableMetricForEntry(
    const std::string& entry_name,
    const std::string& metric_name) const {
  std::vector<std::string> metric_name_vector(1, metric_name);
  std::vector<ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics>
      filtered_result;
  base::ranges::copy_if(
      GetMetrics(entry_name, metric_name_vector),
      std::back_inserter(filtered_result),
      [&metric_name](
          ukm::TestAutoSetUkmRecorder::HumanReadableUkmMetrics metric) {
        if (metric.empty())
          return false;
        return metric.begin()->first == metric_name;
      });
  return filtered_result;
}

TestUkmRecorder::HumanReadableUkmEntry::HumanReadableUkmEntry() = default;

TestUkmRecorder::HumanReadableUkmEntry::HumanReadableUkmEntry(
    ukm::SourceId source_id,
    TestUkmRecorder::HumanReadableUkmMetrics ukm_metrics)
    : source_id(source_id), metrics(std::move(ukm_metrics)) {}

TestUkmRecorder::HumanReadableUkmEntry::HumanReadableUkmEntry(
    const HumanReadableUkmEntry&) = default;
TestUkmRecorder::HumanReadableUkmEntry::~HumanReadableUkmEntry() = default;

bool TestUkmRecorder::HumanReadableUkmEntry::operator==(
    const HumanReadableUkmEntry& other) const {
  return source_id == other.source_id && metrics == other.metrics;
}

void PrintTo(const TestUkmRecorder::HumanReadableUkmEntry& entry,
             std::ostream* os) {
  (*os) << "Entry{source=" << entry.source_id << " ";
  for (const auto& name_value : entry.metrics) {
    (*os) << name_value.first << "=" << name_value.second << ' ';
  }
  (*os) << "}";
}

}  // namespace ukm
