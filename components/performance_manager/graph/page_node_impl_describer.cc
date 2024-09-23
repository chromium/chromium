// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl_describer.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/node_data_describer_util.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace performance_manager {

namespace {

const char kDescriberName[] = "PageNodeImpl";

const char* PermissionStatusToString(
    std::optional<blink::mojom::PermissionStatus> permission_status) {
  if (!permission_status.has_value()) {
    return "undefined";
  }
  switch (permission_status.value()) {
    case blink::mojom::PermissionStatus::ASK:
      return "ask";
    case blink::mojom::PermissionStatus::DENIED:
      return "denied";
    case blink::mojom::PermissionStatus::GRANTED:
      return "granted";
  }
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

base::Value::Dict PageNodeImplDescriber::DescribePageNodeData(
    const PageNode* page_node) const {
  const PageNodeImpl* page_node_impl = PageNodeImpl::FromNode(page_node);
  DCHECK_CALLED_ON_VALID_SEQUENCE(page_node_impl->sequence_checker_);

  base::Value::Dict result;

  result.Set("visibility_change_time",
             TimeDeltaFromNowToValue(page_node_impl->visibility_change_time_));
  if (page_node_impl->audible_change_time_.has_value()) {
    result.Set(
        "audible_change_time",
        TimeDeltaFromNowToValue(page_node_impl->audible_change_time_.value()));
  }
  result.Set(
      "navigation_committed_time",
      TimeDeltaFromNowToValue(page_node_impl->navigation_committed_time_));
  result.Set("has_nonempty_beforeunload",
             page_node_impl->has_nonempty_beforeunload_);
  result.Set("main_frame_url", page_node_impl->main_frame_url_.value().spec());
  result.Set("navigation_id",
             base::NumberToString(page_node_impl->navigation_id_));
  result.Set("contents_mime_type", page_node_impl->contents_mime_type_);
  result.Set("browser_context_id", page_node_impl->browser_context_id_);
  result.Set("type", PageNode::ToString(page_node_impl->type_.value()));
  result.Set("is_visible", page_node_impl->is_visible_.value());
  result.Set("is_audible", page_node_impl->is_audible_.value());
  result.Set("loading_state",
             PageNode::ToString(page_node_impl->loading_state_.value()));
  result.Set("ukm_source_id",
             base::NumberToString(page_node_impl->ukm_source_id_.value()));
  result.Set("lifecycle_state",
             MojoEnumToString(page_node_impl->lifecycle_state_.value()));
  result.Set("is_holding_weblock", page_node_impl->is_holding_weblock_.value());
  result.Set("is_holding_indexeddb_lock",
             page_node_impl->is_holding_indexeddb_lock_.value());
  result.Set("had_form_interaction",
             page_node_impl->had_form_interaction_.value());
  result.Set("had_user_edits", page_node_impl->had_user_edits_.value());
  result.Set("notification_permission",
             PermissionStatusToString(
                 page_node_impl->notification_permission_status_));
  if (page_node_impl->embedding_type_ != PageNode::EmbeddingType::kInvalid) {
    result.Set("embedding_type",
               PageNode::ToString(page_node_impl->embedding_type_));
  }
  result.Set("resource_context",
             page_node_impl->GetResourceContext().ToString());

  base::Value::Dict estimates;
  estimates.Set(
      "private_footprint_kb",
      base::NumberToString(page_node_impl->EstimatePrivateFootprintSize()));
  estimates.Set(
      "resident_set_size_kb",
      base::NumberToString(page_node_impl->EstimateResidentSetSize()));
  result.Set("estimates", std::move(estimates));

  return result;
}

}  // namespace performance_manager
