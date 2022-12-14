// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_state_tracker.h"

#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {
constexpr char kParentAccessFlowResultHistogramBase[] =
    "ChromeOS.FamilyLinkUser.ParentAccess.FlowResult";

// TODO(b/262555804) use shared constants for flow type variant suffixes.
constexpr char kParentAccessFlowResultSuffixAll[] = "All";
constexpr char kParentAccessFlowResultSuffixWebApprovals[] = "WebApprovals";
}  // namespace

// static
std::string ParentAccessStateTracker::GetParentAccessResultHistogramForFlowType(
    absl::optional<parent_access_ui::mojom::ParentAccessParams::FlowType>
        flow_type) {
  const std::string separator = ".";
  if (!flow_type) {
    return base::JoinString({kParentAccessFlowResultHistogramBase,
                             kParentAccessFlowResultSuffixAll},
                            separator);
  }
  switch (flow_type.value()) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      return base::JoinString({kParentAccessFlowResultHistogramBase,
                               kParentAccessFlowResultSuffixWebApprovals},
                              separator);
  }
}

ParentAccessStateTracker::ParentAccessStateTracker(
    parent_access_ui::mojom::ParentAccessParams::FlowType flow_type)
    : flow_type_(flow_type) {
  switch (flow_type_) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      // Initialize flow result to kParentAuthentication for flows without an
      // initial screen.
      flow_result_ = FlowResult::kParentAuthentication;
      break;
  }
}

ParentAccessStateTracker::~ParentAccessStateTracker() {
  base::UmaHistogramEnumeration(
      GetParentAccessResultHistogramForFlowType(absl::nullopt), flow_result_,
      FlowResult::kNumStates);

  switch (flow_type_) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      base::UmaHistogramEnumeration(
          GetParentAccessResultHistogramForFlowType(flow_type_), flow_result_,
          FlowResult::kNumStates);
      break;
  }
}

void ParentAccessStateTracker::OnWebUiStateChanged(FlowResult result) {
  flow_result_ = result;
}
}  // namespace ash
