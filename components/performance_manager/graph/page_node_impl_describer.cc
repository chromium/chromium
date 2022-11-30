// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl_describer.h"

#include "base/strings/string_number_conversions.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/freezing/freezing.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "PageNodeImpl";

const char* FreezingVoteToString(
    absl::optional<freezing::FreezingVote> freezing_vote) {
  if (!freezing_vote)
    return "None";

  return freezing::FreezingVoteValueToString(freezing_vote->value());
}

}  // namespace

PageNodeImplDescriber::PageNodeImplDescriber() = default;
PageNodeImplDescriber::~PageNodeImplDescriber() = default;

void PageNodeImplDescriber::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void PageNodeImplDescriber::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value PageNodeImplDescriber::DescribePageNodeData(
    const PageNode* page_node) const {
  const PageNodeImpl* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK_CALLED_ON_VALID_SEQUENCE(page_node_impl->sequence_checker_);

  base::Value result(base::Value::Type::DICTIONARY);

  result.SetKey(
      "visibility_change_time",
      TimeDeltaFromNowToValue(page_node_impl->visibility_change_time_));
  result.SetKey(
      "navigation_committed_time",
      TimeDeltaFromNowToValue(page_node_impl->navigation_committed_time_));
  result.SetKey("usage_estimate_time",
                TimeDeltaFromNowToValue(page_node_impl->usage_estimate_time_));
  // TODO(pmonette): Instead of emitting a raw number, this could be a human
  //                 readable string. E.g. "14.8 MiB" instead of "14523".
  result.SetStringKey(
      "private_footprint_kb_estimate",
      base::NumberToString(page_node_impl->private_footprint_kb_estimate_));
  result.SetBoolKey("has_nonempty_beforeunload",
                    page_node_impl->has_nonempty_beforeunload_);
  result.SetStringKey("main_frame_url",
                      page_node_impl->main_frame_url_.value().spec());
  result.SetStringKey("navigation_id",
                      base::NumberToString(page_node_impl->navigation_id_));
  result.SetStringKey("contents_mime_type",
                      page_node_impl->contents_mime_type_);
  result.SetStringKey("browser_context_id",
                      page_node_impl->browser_context_id_);
  result.SetStringKey("type",
                      PageNode::ToString(page_node_impl->type_.value()));
  result.SetBoolKey("is_visible", page_node_impl->is_visible_.value());
  result.SetBoolKey("is_audible", page_node_impl->is_audible_.value());
  result.SetStringKey(
      "loading_state",
      PageNode::ToString(page_node_impl->loading_state_.value()));
  result.SetStringKey(
      "ukm_source_id",
      base::NumberToString(page_node_impl->ukm_source_id_.value()));
  result.SetStringKey(
      "lifecycle_state",
      MojoEnumToString(page_node_impl->lifecycle_state_.value()));
  result.SetBoolKey("is_holding_weblock",
                    page_node_impl->is_holding_weblock_.value());
  result.SetBoolKey("is_holding_indexeddb_lock",
                    page_node_impl->is_holding_indexeddb_lock_.value());
  result.SetBoolKey("had_form_interaction",
                    page_node_impl->had_form_interaction_.value());
  if (page_node_impl->embedding_type_ != PageNode::EmbeddingType::kInvalid) {
    result.SetStringKey("embedding_type",
                        PageNode::ToString(page_node_impl->embedding_type_));
  }
  result.SetStringKey("freezing_vote",
                      FreezingVoteToString(page_node_impl->freezing_vote()));

  return result;
}

}  // namespace performance_manager
