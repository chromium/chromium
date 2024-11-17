// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_BASE_H_
#define COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_BASE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "base/component_export.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"

namespace dwa::internal {

// An internal base class for the generated DwaEntry builder objects and the
// DwaEntryBuilder class. DwaEntryBuilder is reserved for the case where it is
// not appropriate to use the auto-generated class. This class should not be
// used directly.
class COMPONENT_EXPORT(DWA) DwaEntryBuilderBase {
 public:
  DwaEntryBuilderBase(const DwaEntryBuilderBase&) = delete;
  DwaEntryBuilderBase(DwaEntryBuilderBase&&);
  DwaEntryBuilderBase& operator=(const DwaEntryBuilderBase&) = delete;
  DwaEntryBuilderBase& operator=(DwaEntryBuilderBase&&);

  virtual ~DwaEntryBuilderBase();

  // Records the complete entry into the recorder. If recorder is null, the
  // entry is simply discarded. The |entry_| is used up by this call so
  // further calls to this will do nothing.
  void Record(metrics::dwa::DwaRecorder* recorder);

  // Return a pointer to internal DwaEntryPtr for testing.
  metrics::dwa::mojom::DwaEntryPtr* GetEntryForTesting();

 protected:
  explicit DwaEntryBuilderBase(uint64_t event_hash);

  // Sets the content to the entry. The content is represented as a
  // hash.
  void SetContentInternal(uint64_t content_hash);

  // Add metric to the entry. A metric contains a metric hash and value.
  void SetMetricInternal(uint64_t metric_hash, int64_t value);

  // Add study name to the set of studies of interest.
  void AddToStudiesOfInterestInternal(std::string_view study_name);

 private:
  metrics::dwa::mojom::DwaEntryPtr entry_;
};

}  // namespace dwa::internal

#endif  // COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_BASE_H_
