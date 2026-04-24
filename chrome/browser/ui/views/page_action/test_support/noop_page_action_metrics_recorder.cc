// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/noop_page_action_metrics_recorder.h"

#include <utility>

#include "base/functional/callback.h"

namespace page_actions {

void NoopPageActionMetricsRecorder::RecordClick(
    actions::ActionId action_id,
    PageActionTrigger trigger_source) {}

void NoopPageActionMetricsRecorder::Observe(
    PageActionModelInterface& model,
    const PageActionProperties& properties) {}

std::unique_ptr<PageActionMetricsRecorderInterface>
NoopPageActionMetricsRecorderFactory::CreateRecorder(
    tabs::TabInterface& tab_interface,
    VisibleEphemeralPageActionsCountCallback
        visible_ephemeral_page_actions_count_callback) {
  return std::make_unique<NoopPageActionMetricsRecorder>();
}

}  // namespace page_actions
