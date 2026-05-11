// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_EVENT_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_EVENT_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <variant>

#include "base/time/time.h"
#include "ui/base/page_transition_types.h"

namespace page_content_annotations {

struct PageStabilityMonitorStartEvent {};

struct PageStabilityMonitorStartDelayEvent {
  base::TimeDelta delay;
};

struct PageStabilityMonitorStopEvent {};

struct PageStabilityMonitorTearDownEvent {};

// The paint stability monitor may be still actively monitoring for interaction
// effects after the stability was reached. The event may still be invoked to
// record metrics after the stability was reached, but the data would be unset.
struct InteractionContentfulPaintEvent {
  struct Data {
    uint64_t total_painted_area;
    uint64_t new_painted_area;
    bool was_stability_reached;
  };

  std::optional<Data> data;
};

struct PaintStabilityMonitorStartedEvent {
  uint64_t initial_painted_area;
};

struct PaintStabilityDetectedEvent {
  uint64_t total_painted_area;
  bool is_waiting_for_stable;
};

struct PaintStabilityReachedEvent {};

struct NetworkIdleEvent {};

struct MainThreadIdleEvent {};

struct NetworkAndMainThreadIdleEvent {};

struct DidCommitProvisionalLoadEvent {
  ui::PageTransition transition;
};

struct DidFailProvisionalLoadEvent {};

struct DidSetPageLifecycleStateEvent {};

struct NetworkAndMainThreadStabilityMonitorCreatedEvent {
  size_t starting_request_count;
};

struct NetworkAndMainThreadStabilityMonitorStartedEvent {
  size_t after_request_count;
};

// A variant type representing all events emitted by the stability monitors.
// This event-based system allows the PageStabilityMonitorDelegate to observe
// fine-grained internal details (like painted area changes or network request
// counts) without the monitors needing to know about the delegate's specific
// logging requirements.
using PageStabilityEvent =
    std::variant<PageStabilityMonitorStartEvent,
                 PageStabilityMonitorStartDelayEvent,
                 PageStabilityMonitorStopEvent,
                 PageStabilityMonitorTearDownEvent,
                 InteractionContentfulPaintEvent,
                 PaintStabilityMonitorStartedEvent,
                 PaintStabilityDetectedEvent,
                 PaintStabilityReachedEvent,
                 NetworkIdleEvent,
                 MainThreadIdleEvent,
                 NetworkAndMainThreadIdleEvent,
                 DidCommitProvisionalLoadEvent,
                 DidFailProvisionalLoadEvent,
                 DidSetPageLifecycleStateEvent,
                 NetworkAndMainThreadStabilityMonitorCreatedEvent,
                 NetworkAndMainThreadStabilityMonitorStartedEvent>;

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_STABILITY_EVENT_H_
