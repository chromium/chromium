// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/contextual_search_session_entry.h"

#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"

namespace contextual_search {

ContextualSearchSessionEntry::ContextualSearchSessionEntry(
    ContextualSearchSessionEntry&&) = default;
ContextualSearchSessionEntry&
ContextualSearchSessionEntry::ContextualSearchSessionEntry::operator=(
    ContextualSearchSessionEntry&&) = default;

ContextualSearchSessionEntry::ContextualSearchSessionEntry(
    std::unique_ptr<ContextualSearchContextController> controller,
    std::unique_ptr<ContextualSearchMetricsRecorder> metrics_recorder)
    : controller_(std::move(controller)),
      metrics_recorder_(std::move(metrics_recorder)) {}

ContextualSearchSessionEntry::~ContextualSearchSessionEntry() = default;

}  // namespace contextual_search
