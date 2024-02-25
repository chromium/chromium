// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_ukm_test_helper.h"
#include "base/memory/raw_ptr.h"

#include <sstream>

#include "base/ranges/algorithm.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Verifies each expected metric's value. Metrics not in |expected_metrics| are
// ignored. A metric value of |nullopt| implies the metric shouldn't exist.
void ExpectEntryMetrics(const ukm::mojom::UkmEntry& entry,
                        const UkmMetricMap& expected_metrics) {
  // Each expected metric should match a named value in the UKM entry.
  for (const UkmMetricMap::value_type& pair : expected_metrics) {
    if (pair.second.has_value()) {
      ukm::TestUkmRecorder::ExpectEntryMetric(&entry, pair.first,
                                              pair.second.value());
    } else {
      // The metric shouldn't exist.
      EXPECT_FALSE(ukm::TestUkmRecorder::EntryHasMetric(&entry, pair.first))
          << " for metric: " << pair.first;
    }
  }
}

// Returns true if each metric in |expected_metrics| has the same value in the
// given entry. An expected metric value of |nullopt| implies a value shouldn't
// exist in the entry.
bool EntryContainsMetrics(const ukm::mojom::UkmEntry* entry,
                          const UkmMetricMap& expected_metrics) {
  for (const UkmMetricMap::value_type& expected_pair : expected_metrics) {
    const int64_t* metric =
        ukm::TestUkmRecorder::GetEntryMetric(entry, expected_pair.first);
    if (expected_pair.second.has_value()) {
      if (!metric || *metric != expected_pair.second.value())
        return false;
    } else {
      // The metric shouldn't exist.
      if (ukm::TestUkmRecorder::EntryHasMetric(entry, expected_pair.first))
        return false;
    }
  }
  return true;
}

// Returns an iterator to an entry whose metrics match |expected_metrics|,
// or end() if not found.
std::vector<
    raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>::const_iterator
FindMatchingEntry(
    const std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>&
        entries,
    const UkmMetricMap& expected_metrics) {
  return base::ranges::find_if(
      entries, [&expected_metrics](const ukm::mojom::UkmEntry* entry) {
        return EntryContainsMetrics(entry, expected_metrics);
      });
}

}  // namespace

UkmEntryChecker::UkmEntryChecker() = default;

UkmEntryChecker::~UkmEntryChecker() {
  // Events under test should not have new, unchecked entries.
  for (const auto& pair : num_entries_) {
    const std::string& entry_name = pair.first;
    int num_unexpected_entries = NumNewEntriesRecorded(entry_name);
    // Could be negative if an expectation has already failed.
    if (num_unexpected_entries <= 0)
      continue;

    ADD_FAILURE() << "Found " << num_unexpected_entries
                  << " unexpected UKM entries at shutdown for: " << entry_name;
    size_t first_unexpected_index = num_entries_[entry_name];
    const ukm::mojom::UkmEntry* ukm_entry =
        ukm_recorder_.GetEntriesByName(entry_name)[first_unexpected_index];

    std::ostringstream entry_metrics;
    for (const auto& metric : ukm_entry->metrics)
      entry_metrics << "\n" << metric.first << ": " << metric.second;
    LOG(ERROR) << "First unexpected entry: " << entry_metrics.str();
  }
}

void UkmEntryChecker::ExpectNewEntry(const std::string& entry_name,
                                     const GURL& source_url,
                                     const UkmMetricMap& expected_metrics) {
  // There should be at least one new entry, which is the one we're checking.
  num_entries_[entry_name]++;
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_.GetEntriesByName(entry_name);
  ASSERT_LE(num_entries_[entry_name], entries.size())
      << "Expected at least " << num_entries_[entry_name] << " entries, found "
      << entries.size() << " for " << entry_name;

  // Verify the entry is associated with the correct URL.
  const ukm::mojom::UkmEntry* entry = entries[num_entries_[entry_name] - 1];
  if (!source_url.is_empty())
    ukm_recorder_.ExpectEntrySourceHasUrl(entry, source_url);

  ExpectEntryMetrics(*entry, expected_metrics);
}

void UkmEntryChecker::ExpectNewEntries(
    const std::string& entry_name,
    const std::vector<UkmMetricMap>& expected_entries) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_.GetEntriesByName(entry_name);

  const size_t num_new_entries = expected_entries.size();
  num_entries_[entry_name] += num_new_entries;
  ASSERT_LE(num_entries_[entry_name], entries.size())
      << "Expected at least " << num_entries_[entry_name] << " entries, found "
      << entries.size() << " for " << entry_name;

  // Remove old entries from |entries| before matching new entries.
  entries.erase(entries.begin(), entries.end() - num_new_entries);
  for (size_t i = 0; i < expected_entries.size(); i++) {
    auto it = FindMatchingEntry(entries, expected_entries[i]);
    if (it == entries.end()) {
      ADD_FAILURE() << "Expected entry " << i << " not found.";
      continue;
    } else {
      // Remove the matched entry from the pool of actual entries.
      entries.erase(it);
    }
  }
}

void UkmEntryChecker::ExpectNewEntriesBySource(
    const std::string& entry_name,
    const SourceUkmMetricMap& expected_data) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_.GetEntriesByName(entry_name);

  const size_t num_new_entries = expected_data.size();
  const size_t num_entries = entries.size();
  num_entries_[entry_name] += num_new_entries;

  ASSERT_LE(num_entries_[entry_name], entries.size());
  std::set<ukm::SourceId> found_source_ids;

  for (size_t i = 0; i < num_new_entries; ++i) {
    const ukm::mojom::UkmEntry* entry =
        entries[num_entries - num_new_entries + i];
    const ukm::SourceId& source_id = entry->source_id;
    const auto& expected_data_for_id = expected_data.find(source_id);
    EXPECT_TRUE(expected_data_for_id != expected_data.end());
    EXPECT_EQ(0u, found_source_ids.count(source_id));

    found_source_ids.insert(source_id);
    const std::pair<GURL, UkmMetricMap>& expected_url_metrics =
        expected_data_for_id->second;

    const GURL& source_url = expected_url_metrics.first;
    const UkmMetricMap& expected_metrics = expected_url_metrics.second;
    if (!source_url.is_empty())
      ukm_recorder_.ExpectEntrySourceHasUrl(entry, source_url);

    // Each expected metric should match a named value in the UKM entry.
    ExpectEntryMetrics(*entry, expected_metrics);
  }
}

int UkmEntryChecker::NumNewEntriesRecorded(
    const std::string& entry_name) const {
  const size_t current_ukm_entries = NumEntries(entry_name);

  // If a value hasn't been inserted for |entry_name|, the test hasn't checked
  // for these entries before, so they all count as new.
  if (!num_entries_.count(entry_name))
    return current_ukm_entries;

  size_t previous_num_entries = num_entries_.at(entry_name);
  EXPECT_GE(current_ukm_entries, previous_num_entries);
  return current_ukm_entries - previous_num_entries;
}

size_t UkmEntryChecker::NumEntries(const std::string& entry_name) const {
  return ukm_recorder_.GetEntriesByName(entry_name).size();
}

const ukm::mojom::UkmEntry* UkmEntryChecker::LastUkmEntry(
    const std::string& entry_name) const {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      ukm_recorder_.GetEntriesByName(entry_name);
  CHECK(!entries.empty());
  return entries.back();
}
