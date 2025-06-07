// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/page_stability_monitor.h"

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "content/public/renderer/render_frame.h"

namespace actor {

PageStabilityMonitor::PageStabilityMonitor(content::RenderFrame& frame)
    : frame_(frame) {}

PageStabilityMonitor::~PageStabilityMonitor() = default;

void PageStabilityMonitor::WaitForStable(base::OnceClosure callback) {
  // TODO(crbug.com/414662842): For now just use the observation delay. This is
  // migrated from the browser. Soon this class will look at signals from blink
  // rather than (or in addition to) this delay.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback),
      features::kGlicActorActorObservationDelay.Get());
}

}  // namespace actor
