// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_metrics_utils.h"
#include "base/strings/string_util.h"

namespace parent_access {

namespace {
constexpr char kParentAccessSuffixAll[] = "All";
constexpr char kParentAccessSuffixExtensionApprovals[] = "ExtensionApprovals";
constexpr char kParentAccessSuffixWebApprovals[] = "WebApprovals";
}  // namespace

std::string GetHistogramTitleForFlowType(
    std::string_view parent_access_histogram_base,
    std::optional<parent_access_ui::mojom::ParentAccessParams::FlowType>
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
      return base::JoinString(
          {parent_access_histogram_base, kParentAccessSuffixExtensionApprovals},
          separator);
  }
}
}  // namespace parent_access
