// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl_describer.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/values.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "FrameNodeImpl";

std::string ViewportIntersectionToString(
    const absl::optional<gfx::Rect>& viewport_intersection) {
  if (!viewport_intersection.has_value())
    return "Nullopt";

  return viewport_intersection->ToString();
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

base::Value FrameNodeImplDescriber::DescribeFrameNodeData(
    const FrameNode* node) const {
  const FrameNodeImpl* impl = FrameNodeImpl::FromNode(node);

  base::Value ret(base::Value::Type::DICTIONARY);

  // Document specific properties. These are emitted in a nested dictionary, as
  // a frame node can be reused for different documents.
  base::Value doc(base::Value::Type::DICTIONARY);
  doc.SetStringKey("url", impl->document_.url.value().possibly_invalid_spec());
  doc.SetBoolKey("has_nonempty_beforeunload",
                 impl->document_.has_nonempty_beforeunload);
  doc.SetBoolKey("network_almost_idle",
                 impl->document_.network_almost_idle.value());
  doc.SetBoolKey("had_form_interaction",
                 impl->document_.had_form_interaction.value());
  ret.SetKey("document", std::move(doc));

  // Frame node properties.
  ret.SetIntKey("render_frame_id", impl->render_frame_id_);
  ret.SetStringKey("frame_token", impl->frame_token_.value().ToString());
  ret.SetIntKey("browsing_instance_id", impl->browsing_instance_id_.value());
  ret.SetIntKey("site_instance_id", impl->site_instance_id_.value());
  ret.SetStringKey("lifecycle_state",
                   MojoEnumToString(impl->lifecycle_state_.value()));
  ret.SetBoolKey("is_ad_frame", impl->is_ad_frame_.value());
  ret.SetBoolKey("is_holding_weblock", impl->is_holding_weblock_.value());
  ret.SetBoolKey("is_holding_indexeddb_lock",
                 impl->is_holding_indexeddb_lock_.value());
  ret.SetBoolKey("is_current", impl->is_current_.value());
  ret.SetKey("priority",
             PriorityAndReasonToValue(impl->priority_and_reason_.value()));
  ret.SetBoolKey("is_audible", impl->is_audible_.value());
  ret.SetStringKey(
      "viewport_intersection",
      ViewportIntersectionToString(impl->viewport_intersection_.value()));
  ret.SetStringKey("visibility",
                   FrameNodeVisibilityToString(impl->visibility_.value()));

  return ret;
}

}  // namespace performance_manager
