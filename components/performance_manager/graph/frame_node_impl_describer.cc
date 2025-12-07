// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl_describer.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "FrameNodeImpl";

std::string ViewportIntersectionToString(
    ViewportIntersection viewport_intersection) {
  switch (viewport_intersection) {
    case ViewportIntersection::kUnknown:
      return "Unknown";
    case ViewportIntersection::kNotIntersecting:
      return "Not intersecting";
    case ViewportIntersection::kIntersecting:
      return "Intersecting";
  }
  NOTREACHED();
}

}  // namespace

FrameNodeImplDescriber::~FrameNodeImplDescriber() = default;

void FrameNodeImplDescriber::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void FrameNodeImplDescriber::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value::Dict FrameNodeImplDescriber::DescribeFrameNodeData(
    const FrameNode* node) const {
  const FrameNodeImpl* impl = FrameNodeImpl::FromNode(node);

  base::Value::Dict ret;

  // Document specific properties. These are emitted in a nested dictionary, as
  // a frame node can be reused for different documents.
  base::Value::Dict doc;
  doc.Set("url", impl->document_.url.possibly_invalid_spec());
  doc.Set("origin", impl->document_.origin.has_value()
                        ? impl->document_.origin->GetDebugString()
                        : "undefined");
  doc.Set("has_nonempty_beforeunload",
          impl->document_.has_nonempty_beforeunload);
  doc.Set("network_almost_idle", impl->document_.network_almost_idle.value());
  doc.Set("had_form_interaction", impl->document_.had_form_interaction.value());
  doc.Set("had_user_edits", impl->document_.had_user_edits.value());
  doc.Set("uses_webrtc", impl->document_.uses_web_rtc.value());
  ret.Set("has_freezing_origin_trial_opt_out",
          impl->document_.has_freezing_origin_trial_opt_out.value());
  ret.Set("document", std::move(doc));

  // Frame node properties.
  ret.Set("render_frame_id", impl->render_frame_id_);
  ret.Set("frame_token", impl->frame_token_.value().ToString());
  ret.Set("browsing_instance_id", impl->browsing_instance_id_.value());
  ret.Set("site_instance_group_id", impl->site_instance_group_id_.value());
  ret.Set("lifecycle_state", MojoEnumToString(impl->lifecycle_state_.value()));
  ret.Set("is_ad_frame", impl->is_ad_frame_.value());
  ret.Set("is_holding_weblock", impl->is_holding_weblock_.value());
  ret.Set("is_holding_blocking_indexeddb_lock",
          impl->is_holding_blocking_indexeddb_lock_.value());
  ret.Set("is_current", impl->IsCurrent());
  ret.Set("priority", PriorityAndReasonToValue(impl->GetPriorityAndReason()));
  ret.Set("is_audible", impl->is_audible_.value());
  ret.Set("is_capturing_media_stream",
          impl->is_capturing_media_stream_.value());
  ret.Set("viewport_intersection",
          ViewportIntersectionToString(impl->viewport_intersection_.value()));
  ret.Set("visibility", impl->visibility_->ToString());
  ret.Set("is_intersecting_large_area", impl->IsIntersectingLargeArea());
  ret.Set("is_important", impl->is_important_.value());
  ret.Set("resource_context", impl->GetResourceContext().ToString());

  base::Value::Dict metrics;
  metrics.Set("resident_set",
              base::NumberToString(impl->GetResidentSetEstimate().InKiB()));
  metrics.Set(
      "private_footprint",
      base::NumberToString(impl->GetPrivateFootprintEstimate().InKiB()));
  ret.Set("metrics_estimates", std::move(metrics));

  return ret;
}

}  // namespace performance_manager
