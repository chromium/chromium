// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_ENTRY_FILTER_H_
#define COMPONENTS_UKM_UKM_ENTRY_FILTER_H_

#include <cstdint>

#include "base/containers/flat_set.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"

namespace ukm {

// Used in conjunction with UkmService. See UkmService::RegisterEntryFilter().
class UkmEntryFilter {
 public:
  virtual ~UkmEntryFilter() = default;

  // UkmService invokes this method on every reported entry. |entry| is mutable.
  //
  // An implementation:
  //
  //   * MAY modify the content of |entry->metrics| to remove unnecessary or
  //     prohibited metrics.
  //
  //     Hashes of metrics that should be recorded as having been removed by the
  //     filter should be added to |removed_metric_hashes|.
  //
  //     A filter may exclude a removed metric from |removed_metric_hashes| to
  //     prevent a removed metric from being counted in the aggregate tables.
  //     This is intentional and intends to allow removal of privacy sensitive
  //     metrics without the those metrics being partially exposed via aggregate
  //     tables.
  //
  //   * MUST NOT modify the |source_id| nor |event_hash|.
  //
  // There could be more than one UkmEntryFilter. The probihition on
  // modifications to |source| and |event_hash|, and adding new metrics is due
  // to the possibility that the new values not being seen by previously invoked
  // UkmEntryFilters.
  //
  // Returning false drops the entire entry.
  virtual bool FilterEntry(mojom::UkmEntry* entry,
                           base::flat_set<uint64_t>* removed_metric_hashes) = 0;

  // Invoked each time UkmService constructs a client report and stores all
  // accumulated recordings in it. Effectively signals the start of a new
  // report.
  virtual void OnStoreRecordingsInReport() {}
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_ENTRY_FILTER_H_
