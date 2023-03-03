// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "base/strings/string_util.h"

namespace parent_access {

namespace {
constexpr char kParentAccessSuffixAll[] = "All";
constexpr char kParentAccessSuffixWebApprovals[] = "WebApprovals";
}  // namespace

std::string GetHistogramTitleForFlowType(
    base::StringPiece parent_access_histogram_base,
    absl::optional<parent_access_ui::mojom::ParentAccessParams::FlowType>
        flow_type) {
  const std::string separator = ".";
  if (!flow_type.has_value()) {
    return base::JoinString(
        {parent_access_histogram_base, kParentAccessSuffixAll}, separator);
  }
  switch (flow_type.value()) {
    case parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess:
      return base::JoinString(
          {parent_access_histogram_base, kParentAccessSuffixWebApprovals},
          separator);
    case parent_access_ui::mojom::ParentAccessParams::FlowType::
        kExtensionAccess:
      // TODO(b/262451256): Implement metrics for extension flow.
      return std::string();
  }
}
}  // namespace parent_access
