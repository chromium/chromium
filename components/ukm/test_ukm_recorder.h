// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_RECORDER_H_
#define COMPONENTS_UKM_TEST_UKM_RECORDER_H_

#include <stddef.h>

#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {

// Wraps an UkmRecorder with additional accessors used for testing.
class TestUkmRecorder : public UkmRecorderImpl {
 public:
  using HumanReadableUkmMetrics = std::map<std::string, int64_t>;

  struct HumanReadableUkmEntry {
    HumanReadableUkmEntry();
    HumanReadableUkmEntry(ukm::SourceId source_id,
                          HumanReadableUkmMetrics ukm_metrics);
    ~HumanReadableUkmEntry();
    HumanReadableUkmEntry(const HumanReadableUkmEntry&);

    bool operator==(const HumanReadableUkmEntry& other) const;

    ukm::SourceId source_id = kInvalidSourceId;
    HumanReadableUkmMetrics metrics;
  };

  TestUkmRecorder();

  TestUkmRecorder(const TestUkmRecorder&) = delete;
  TestUkmRecorder& operator=(const TestUkmRecorder&) = delete;

  ~TestUkmRecorder() override;

  void AddEntry(mojom::UkmEntryPtr entry) override;

  size_t sources_count() const { return sources().size(); }

  size_t entries_count() const { return entries().size(); }

  using UkmRecorderImpl::UpdateSourceURL;
  using UkmRecorderImpl::RecordOtherURL;

  // Gets all recorded UkmSource data.
  const std::map<ukm::SourceId, std::unique_ptr<UkmSource>>& GetSources()
      const {
    return sources();
  }

  // Gets UkmSource data for a single SourceId. Returns null if not found.
  const UkmSource* GetSourceForSourceId(ukm::SourceId source_id) const;

  // Gets DocumentCreatedEntry for a single SourceId. Returns null if not found.
  const ukm::mojom::UkmEntry* GetDocumentCreatedEntryForSourceId(
      ukm::SourceId source_id) const;

  // Sets a callback that will be called when recording an entry for entry name.
  void SetOnAddEntryCallback(std::string_view entry_name,
                             base::RepeatingClosure on_add_entry);

  // Gets all of the entries recorded for entry name.
  std::vector<raw_ptr<const mojom::UkmEntry, VectorExperimental>>
  GetEntriesByName(std::string_view entry_name) const;

  // Gets the data for all entries with given entry name, merged to one entry
  // for each source id. Intended for singular="true" metrics.
  std::map<ukm::SourceId, mojom::UkmEntryPtr> GetMergedEntriesByName(
      std::string_view entry_name) const;

  // Checks if an entry is associated with a url.
  void ExpectEntrySourceHasUrl(const mojom::UkmEntry* entry,
                               const GURL& url) const;

  // Expects the value of a metric from an entry.
  static void ExpectEntryMetric(const mojom::UkmEntry* entry,
                                std::string_view metric_name,
                                int64_t expected_value);

  // Checks if an entry contains a specific metric.
  static bool EntryHasMetric(const mojom::UkmEntry* entry,
                             std::string_view metric_name);

  // Gets the value of a metric from an entry. Returns nullptr if the metric is
  // not found.
  static const int64_t* GetEntryMetric(const mojom::UkmEntry* entry,
                                       std::string_view metric_name);

  // A test helper returning all metrics for all entries with a given name in a
  // human-readable form, allowing to write clearer test expectations.
  std::vector<HumanReadableUkmMetrics> GetMetrics(
      std::string entry_name,
      const std::vector<std::string>& metric_names) const;

  // Returns the values of the metrics with the passed-in metric name in entries
  // with the passed-in entry name.
  std::vector<int64_t> GetMetricsEntryValues(
      const std::string& entry_name,
      const std::string& metric_name) const;

  // A test helper returning all entries for a given name in a human-readable
  // form, allowing to write clearer test expectations.
  std::vector<HumanReadableUkmEntry> GetEntries(
      std::string entry_name,
      const std::vector<std::string>& metric_names) const;

  // A test helper returning all logged metrics with the given |metric_name| for
  // the entry with the given |entry_name|, filtered to remove any empty
  // HumanReadableUkmEntry results.
  std::vector<HumanReadableUkmMetrics> FilteredHumanReadableMetricForEntry(
      const std::string& entry_name,
      const std::string& metric_name) const;

 private:
  uint64_t entry_hash_to_wait_for_ = 0;
  base::RepeatingClosure on_add_entry_;
};

// Similar to a TestUkmRecorder, but also sets itself as the global UkmRecorder
// on construction, and unsets itself on destruction.
class TestAutoSetUkmRecorder : public TestUkmRecorder {
 public:
  TestAutoSetUkmRecorder();
  ~TestAutoSetUkmRecorder() override;

 private:
  base::WeakPtrFactory<TestAutoSetUkmRecorder> self_ptr_factory_{this};
};

// Formatter method for Google Test.
void PrintTo(const TestUkmRecorder::HumanReadableUkmEntry& entry,
             std::ostream* os);

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_RECORDER_H_
