// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_RECORDER_H_
#define COMPONENTS_UKM_TEST_UKM_RECORDER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace ukm {

// Wraps an UkmRecorder with additional accessors used for testing.
class TestUkmRecorder : public UkmRecorderImpl {
 public:
  TestUkmRecorder();
  ~TestUkmRecorder() override;

  bool ShouldRestrictToWhitelistedSourceIds() const override;
  bool ShouldRestrictToWhitelistedEntries() const override;

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
  void SetOnAddEntryCallback(base::StringPiece entry_name,
                             base::OnceClosure on_add_entry);

  // Gets all of the entries recorded for entry name.
  std::vector<const mojom::UkmEntry*> GetEntriesByName(
      base::StringPiece entry_name) const;

  // Gets the data for all entries with given entry name, merged to one entry
  // for each source id. Intended for singular="true" metrics.
  std::map<ukm::SourceId, mojom::UkmEntryPtr> GetMergedEntriesByName(
      base::StringPiece entry_name) const;

  // Checks if an entry is associated with a url.
  void ExpectEntrySourceHasUrl(const mojom::UkmEntry* entry,
                               const GURL& url) const;

  // Expects the value of a metric from an entry.
  static void ExpectEntryMetric(const mojom::UkmEntry* entry,
                                base::StringPiece metric_name,
                                int64_t expected_value);

  // Checks if an entry contains a specific metric.
  static bool EntryHasMetric(const mojom::UkmEntry* entry,
                             base::StringPiece metric_name);

  // Gets the value of a metric from an entry. Returns nullptr if the metric is
  // not found.
  static const int64_t* GetEntryMetric(const mojom::UkmEntry* entry,
                                       base::StringPiece metric_name);

 private:
  uint64_t entry_hash_to_wait_for_ = 0;
  base::OnceClosure on_add_entry_;

  DISALLOW_COPY_AND_ASSIGN(TestUkmRecorder);
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

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_RECORDER_H_
