// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_METRICS_H_

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"

namespace action_chips {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ActionChipsRequestStatus)
enum class ActionChipsRequestStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kHttpError = 2,
  kParseError = 3,
  kMaxValue = kParseError,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ActionChipsRequestStatus)

void RecordActionChipsRequestStatus(
    const RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult& result);

// Records impression metrics for the generated chips.
void RecordImpressionMetrics(
    const std::vector<action_chips::mojom::ActionChipPtr>& chips);

// Records latency metrics for action chips retrieval.
void RecordActionChipsRetrievalLatencyMetrics(base::TimeDelta latency);

}  // namespace action_chips

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_ACTION_CHIPS_ACTION_CHIPS_METRICS_H_
