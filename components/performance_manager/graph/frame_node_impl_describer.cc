// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl_describer.h"

#include <sstream>
#include <string>

#include "base/task/task_traits.h"
#include "base/values.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "FrameNodeImpl";

// TODO(1077305): Move the following to a public describer_utils helper.

// Mojo enums have std::stream support. This converts them to a std::string.
template <typename MojoEnum>
std::string MojoEnumToString(MojoEnum mojo_enum_value) {
  std::stringstream ss;
  ss << mojo_enum_value;
  return ss.str();
}

// Converts a string to a base::Value, where null strings go to a null value
// instead of an empty string.
base::Value MaybeNullStringToValue(base::StringPiece str) {
  if (str.data() == nullptr)
    return base::Value();
  return base::Value(str);
}

base::Value PriorityAndReasonToValue(
    const execution_context_priority::PriorityAndReason& priority_and_reason) {
  base::Value priority(base::Value::Type::DICTIONARY);
  priority.SetStringKey(
      "priority", base::TaskPriorityToString(priority_and_reason.priority()));
  priority.SetPath("reason",
                   MaybeNullStringToValue(priority_and_reason.reason()));
  return priority;
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
  doc.SetStringKey(
      "origin_trial_freeze_policy",
      MojoEnumToString(impl->document_.origin_trial_freeze_policy.value()));
  doc.SetBoolKey("had_form_interaction",
                 impl->document_.had_form_interaction.value());
  ret.SetKey("document", std::move(doc));

  // Frame node properties.
  ret.SetIntKey("frame_tree_node_id", impl->frame_tree_node_id_);
  ret.SetIntKey("render_frame_id", impl->render_frame_id_);
  ret.SetStringKey("frame_token", impl->frame_token_.value().ToString());
  ret.SetIntKey("browsing_instance_id", impl->browsing_instance_id_);
  ret.SetIntKey("site_instance_id", impl->site_instance_id_);
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

  return ret;
}

}  // namespace performance_manager
