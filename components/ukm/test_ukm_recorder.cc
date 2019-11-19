// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/test_ukm_recorder.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
  EnableRecording(/*extensions=*/true);
  StoreWhitelistedEntries();
  DisableSamplingForTesting();
}

TestUkmRecorder::~TestUkmRecorder() {}

bool TestUkmRecorder::ShouldRestrictToWhitelistedSourceIds() const {
  // In tests, we want to record all source ids (not just those that are
  // whitelisted).
  return false;
}

bool TestUkmRecorder::ShouldRestrictToWhitelistedEntries() const {
  // In tests, we want to record all entries (not just those that are
  // whitelisted).
  return false;
}

void TestUkmRecorder::AddEntry(mojom::UkmEntryPtr entry) {
  const bool should_run_callback =
      on_add_entry_ && entry && entry_hash_to_wait_for_ == entry->event_hash;
  UkmRecorderImpl::AddEntry(std::move(entry));
  if (should_run_callback)
    std::move(on_add_entry_).Run();
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
  for (auto* entry : entries) {
    if (entry->source_id == source_id)
      return entry;
  }
  return nullptr;
}

void TestUkmRecorder::SetOnAddEntryCallback(base::StringPiece entry_name,
                                            base::OnceClosure on_add_entry) {
  on_add_entry_ = std::move(on_add_entry);
  entry_hash_to_wait_for_ = base::HashMetricName(entry_name);
}

std::vector<const mojom::UkmEntry*> TestUkmRecorder::GetEntriesByName(
    base::StringPiece entry_name) const {
  uint64_t hash = base::HashMetricName(entry_name);
  std::vector<const mojom::UkmEntry*> result;
  for (const auto& it : entries()) {
    if (it->event_hash == hash)
      result.push_back(it.get());
  }
  return result;
}

std::map<ukm::SourceId, mojom::UkmEntryPtr>
TestUkmRecorder::GetMergedEntriesByName(base::StringPiece entry_name) const {
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
    return;
  }
  EXPECT_EQ(src->url(), url);
}

// static
bool TestUkmRecorder::EntryHasMetric(const mojom::UkmEntry* entry,
                                     base::StringPiece metric_name) {
  return GetEntryMetric(entry, metric_name) != nullptr;
}

// static
const int64_t* TestUkmRecorder::GetEntryMetric(const mojom::UkmEntry* entry,
                                               base::StringPiece metric_name) {
  uint64_t hash = base::HashMetricName(metric_name);
  const auto it = entry->metrics.find(hash);
  if (it != entry->metrics.end())
    return &it->second;
  return nullptr;
}

// static
void TestUkmRecorder::ExpectEntryMetric(const mojom::UkmEntry* entry,
                                        base::StringPiece metric_name,
                                        int64_t expected_value) {
  const int64_t* metric = GetEntryMetric(entry, metric_name);
  if (metric == nullptr) {
    FAIL() << "Failed to find metric for event: " << metric_name;
    return;
  }
  EXPECT_EQ(expected_value, *metric) << " for metric:" << metric_name;
}

TestAutoSetUkmRecorder::TestAutoSetUkmRecorder() {
  DelegatingUkmRecorder::Get()->AddDelegate(self_ptr_factory_.GetWeakPtr());
}

TestAutoSetUkmRecorder::~TestAutoSetUkmRecorder() {
  DelegatingUkmRecorder::Get()->RemoveDelegate(this);
}

}  // namespace ukm
