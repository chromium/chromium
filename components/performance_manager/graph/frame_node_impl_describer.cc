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

std::string IntersectsViewportToString(
    absl::optional<bool> intersects_viewport) {
  if (!intersects_viewport.has_value()) {
    return "Nullopt";
  }

  return intersects_viewport.value() ? "true" : "false";
}

std::string FrameNodeVisibilityToString(FrameNode::Visibility visibility) {
  switch (visibility) {
    // using FrameNode::Visibility;
    case FrameNode::Visibility::kUnknown:
      return "Unknown";
    case FrameNode::Visibility::kVisible:
      return "Visible";
    case FrameNode::Visibility::kNotVisible:
      return "Not visible";
  }
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
  doc.Set("url", impl->document_.url.value().possibly_invalid_spec());
  doc.Set("has_nonempty_beforeunload",
          impl->document_.has_nonempty_beforeunload);
  doc.Set("network_almost_idle", impl->document_.network_almost_idle.value());
  doc.Set("had_form_interaction", impl->document_.had_form_interaction.value());
  ret.Set("had_user_edits", impl->document_.had_user_edits.value());
  ret.Set("document", std::move(doc));

  // Frame node properties.
  ret.Set("render_frame_id", impl->render_frame_id_);
  ret.Set("frame_token", impl->frame_token_.value().ToString());
  ret.Set("browsing_instance_id", impl->browsing_instance_id_.value());
  ret.Set("site_instance_id", impl->site_instance_id_.value());
  ret.Set("lifecycle_state", MojoEnumToString(impl->lifecycle_state_.value()));
  ret.Set("is_ad_frame", impl->is_ad_frame_.value());
  ret.Set("is_holding_weblock", impl->is_holding_weblock_.value());
  ret.Set("is_holding_indexeddb_lock",
          impl->is_holding_indexeddb_lock_.value());
  ret.Set("is_current", impl->is_current_.value());
  ret.Set("priority", PriorityAndReasonToValue(impl->GetPriorityAndReason()));
  ret.Set("is_audible", impl->is_audible_.value());
  ret.Set("is_capturing_media_stream",
          impl->is_capturing_media_stream_.value());
  ret.Set("viewport_intersection",
          IntersectsViewportToString(impl->intersects_viewport_.value()));
  ret.Set("visibility", FrameNodeVisibilityToString(impl->visibility_.value()));
  ret.Set("resource_context", impl->GetResourceContext().ToString());

  base::Value::Dict metrics;
  metrics.Set("resident_set",
              base::NumberToString(impl->GetResidentSetKbEstimate()));
  metrics.Set("private_footprint",
              base::NumberToString(impl->GetPrivateFootprintKbEstimate()));
  ret.Set("metrics_estimates", std::move(metrics));

  return ret;
}

}  // namespace performance_manager
