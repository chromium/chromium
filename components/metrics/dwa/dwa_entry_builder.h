// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_H_
#define COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_H_

#include <string_view>

#include "base/component_export.h"
#include "components/metrics/dwa/dwa_entry_builder_base.h"

namespace dwa {

// A generic builder object for recording entries in a DwaRecorder, when the
// recording code does not statically know the names of the events/metrics.
// Metrics must still be described in dwa.xml, and this will trigger a DCHECK
// if used to record metrics not described there.
//
// Where possible, prefer using generated objects from dwa_builders.h in the
// dwa::builders namespace instead.
//
// The example usage is:
// dwa::DwaEntryBuilder builder("PageLoad");
// builder.SetContent("Content")
// builder.SetMetric("NavigationStart", navigation_start_time);
// builder.SetMetric("FirstPaint", first_paint_time);
// builder.AddToStudiesOfInterest("Study1");
// builder.Record(dwa_recorder);
class COMPONENT_EXPORT(DWA) DwaEntryBuilder final
    : public dwa::internal::DwaEntryBuilderBase {
 public:
  explicit DwaEntryBuilder(std::string_view event_name);

  DwaEntryBuilder(const DwaEntryBuilder&) = delete;
  DwaEntryBuilder& operator=(const DwaEntryBuilder&) = delete;

  ~DwaEntryBuilder() override;

  void SetContent(std::string_view content);
  void SetMetric(std::string_view metric_name, int64_t value);

  void AddToStudiesOfInterest(std::string_view study_name);
};

}  // namespace dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_ENTRY_BUILDER_H_
