// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_state_tracker.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

ParentAccessStateTracker::ParentAccessStateTracker(
    parent_access_ui::mojom::ParentAccessParams::FlowType flow_type)
    : flow_type_(flow_type) {
  switch (flow_type_) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      // Initialize flow result to kParentAuthentication for flows without an
      // initial screen.
      flow_result_ = FlowResult::kParentAuthentication;
      break;
    case parent_access_ui::mojom::ParentAccessParams::FlowType::
        kExtensionAccess:
      flow_result_ = FlowResult::kInitial;
      break;
  }
}

ParentAccessStateTracker::~ParentAccessStateTracker() {
  base::UmaHistogramEnumeration(
      parent_access::GetHistogramTitleForFlowType(
          parent_access::kParentAccessFlowResultHistogramBase, absl::nullopt),
      flow_result_, FlowResult::kNumStates);

  switch (flow_type_) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      base::UmaHistogramEnumeration(
          parent_access::GetHistogramTitleForFlowType(
              parent_access::kParentAccessFlowResultHistogramBase, flow_type_),
          flow_result_, FlowResult::kNumStates);
      break;
    case parent_access_ui::mojom::ParentAccessParams::FlowType::
        kExtensionAccess:
      // TODO(b/262451256): Implement metrics for extension flow.
      break;
  }
}

void ParentAccessStateTracker::OnWebUiStateChanged(FlowResult result) {
  flow_result_ = result;
}
}  // namespace ash
