// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_METRICS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_METRICS_UTILS_H_

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace parent_access {

// Title bases used in histogram title generation.
constexpr char kParentAccessWidgetShowDialogErrorHistogramBase[] =
    "ChromeOS.FamilyLinkUser.ParentAccessWidgetShowDialogError";
constexpr char kParentAccessFlowResultHistogramBase[] =
    "ChromeOS.FamilyLinkUser.ParentAccess.FlowResult";
constexpr char kParentAccessWidgetErrorHistogramBase[] =
    "ChromeOS.FamilyLinkUser.ParentAccessWidgetError";

// Returns the title string for a histogram given a title base and flow
// type.
std::string GetHistogramTitleForFlowType(
    base::StringPiece parent_access_histogram_base,
    absl::optional<parent_access_ui::mojom::ParentAccessParams::FlowType>
        flow_type);
}  // namespace parent_access

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_PARENT_ACCESS_PARENT_ACCESS_METRICS_UTILS_H_
