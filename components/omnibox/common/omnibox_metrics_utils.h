// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_
#define COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_

#include <string>

#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"

namespace omnibox {

// Tracks the context type.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ContextType)
enum class ContextType {
  kTab = 0,
  kFile = 1,
  kImage = 2,
  kImageGen = 3,
  kDeepResearch = 4,
  kCanvas = 5,
  kAutoModel = 6,
  kThinkingModel = 7,
  kRegularModel = 8,
  kProNoGenUiModel = 9,
  kUnknown = 10,
  kDrive = 11,
  kMaxValue = kDrive,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextType,
// //ui/webui/resources/cr_components/composebox/common.ts:ContextType,
// //tools/metrics/actions/actions.xml:ContextType)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ResultToContentReadyEarlyExitReason)
enum class ResultToContentReadyEarlyExitReason {
  kUnspecified = 0,
  kNoResultReadyTime = 1,
  kVisualStateNotReady = 2,
  kMaxValue = kVisualStateNotReady,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/omnibox/enums.xml:OmniboxPopupResultToContentReadyEarlyExitReason)

std::string GetToolModeString(omnibox::ToolMode mode);

std::string GetModelModeString(omnibox::ModelMode mode);

std::string GetContextTypeString(ContextType type);

void LogResultToContentReadyEarlyExitReason(
    ResultToContentReadyEarlyExitReason reason);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_COMMON_OMNIBOX_METRICS_UTILS_H_
