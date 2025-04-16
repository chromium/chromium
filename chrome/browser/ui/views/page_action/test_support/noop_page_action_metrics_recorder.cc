// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/test_support/noop_page_action_metrics_recorder.h"

namespace page_actions {

std::unique_ptr<PageActionMetricsRecorderInterface>
NoopPageActionMetricsRecorderFactory::Create(
    tabs::TabInterface& tab_interface,
    const PageActionProperties& properties,
    PageActionModelInterface& model) {
  return std::make_unique<NoopPageActionMetricsRecorder>();
}

}  // namespace page_actions
