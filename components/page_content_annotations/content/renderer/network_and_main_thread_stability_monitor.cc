// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/renderer/network_and_main_thread_stability_monitor.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "components/page_content_annotations/content/renderer/page_stability_monitor_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace page_content_annotations {

NetworkAndMainThreadStabilityMonitor::NetworkAndMainThreadStabilityMonitor(
    content::RenderFrame& render_frame,
    PageStabilityMonitorDelegate* delegate)
    : render_frame_(render_frame), delegate_(delegate) {
  CHECK(render_frame_->GetWebFrame());
  starting_request_count_ =
      render_frame_->GetWebFrame()->GetDocument().ActiveResourceRequestCount();

  if (delegate_) {
    delegate_->OnEvent(NetworkAndMainThreadStabilityMonitorCreatedEvent{
        .starting_request_count = starting_request_count_});
  }
}

NetworkAndMainThreadStabilityMonitor::~NetworkAndMainThreadStabilityMonitor() =
    default;

void NetworkAndMainThreadStabilityMonitor::WaitForStable(
    base::OnceClosure callback) {
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);

  size_t after_request_count =
      render_frame_->GetWebFrame()->GetDocument().ActiveResourceRequestCount();
  if (delegate_) {
    delegate_->OnEvent(NetworkAndMainThreadStabilityMonitorStartedEvent{
        .after_request_count = after_request_count});
  }

  if (after_request_count > starting_request_count_) {
    render_frame_->GetWebFrame()->RequestNetworkIdleCallback(
        base::BindOnce(&NetworkAndMainThreadStabilityMonitor::OnNetworkIdle,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    WaitForMainThreadIdle();
  }
}

void NetworkAndMainThreadStabilityMonitor::OnNetworkIdle() {
  if (delegate_) {
    delegate_->OnEvent(NetworkIdleEvent());
  }
  WaitForMainThreadIdle();
}

void NetworkAndMainThreadStabilityMonitor::WaitForMainThreadIdle() {
  render_frame_->GetWebFrame()->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle(base::TimeTicks) {
  if (delegate_) {
    delegate_->OnEvent(MainThreadIdleEvent());
  }

  CHECK(is_stable_callback_);
  std::move(is_stable_callback_).Run();
}

}  // namespace page_content_annotations
