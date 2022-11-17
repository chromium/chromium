// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_load_timer.h"

#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

// Extract UMA_HISTOGRAM_TIMES to allow multiple calls from the same location
// with different names. This skips some optimization, but we don't expect to
// call this frequently.
void CallUmaHistogramTimes(const std::string& name, base::TimeDelta duration) {
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(1), base::Seconds(10), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK(histogram);
  histogram->AddTime(duration);
}

}  // namespace

WebuiLoadTimer::WebuiLoadTimer(
    content::WebContents* web_contents,
    const std::string& document_initial_load_uma_id,
    const std::string& document_load_completed_uma_id)
    : content::WebContentsObserver(web_contents),
      document_initial_load_uma_id_(document_initial_load_uma_id),
      document_load_completed_uma_id_(document_load_completed_uma_id) {
  DCHECK(!document_initial_load_uma_id_.empty());
  DCHECK(!document_load_completed_uma_id_.empty());
}

WebuiLoadTimer::~WebuiLoadTimer() = default;

void WebuiLoadTimer::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  timer_ = std::make_unique<base::ElapsedTimer>();
}

void WebuiLoadTimer::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // See comment in DocumentOnLoadCompletedInPrimaryMainFrame.
  if (!timer_ || !render_frame_host->IsInPrimaryMainFrame())
    return;
  CallUmaHistogramTimes(document_initial_load_uma_id_, timer_->Elapsed());
}

void WebuiLoadTimer::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // The WebContents could have been created for a child RenderFrameHost so it
  // would never receive a DidStartNavigation with the main frame, however it
  // will receive this callback.
  if (!timer_)
    return;
  CallUmaHistogramTimes(document_load_completed_uma_id_, timer_->Elapsed());
  timer_.reset();
}
