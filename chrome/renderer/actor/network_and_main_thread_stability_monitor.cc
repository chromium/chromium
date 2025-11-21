// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/actor/network_and_main_thread_stability_monitor.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/renderer/actor/journal.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace actor {

NetworkAndMainThreadStabilityMonitor::NetworkAndMainThreadStabilityMonitor(
    content::RenderFrame& render_frame,
    TaskId task_id,
    Journal& journal)
    : render_frame_(render_frame), task_id_(task_id), journal_(journal) {
  CHECK(render_frame_->GetWebFrame());
  starting_request_count_ =
      render_frame_->GetWebFrame()->GetDocument().ActiveResourceRequestCount();

  journal_->Log(task_id_, "NetworkAndMainThreadStabilityMonitor: Created",
                JournalDetailsBuilder()
                    .Add("requests_before", starting_request_count_)
                    .Build());
}

NetworkAndMainThreadStabilityMonitor::~NetworkAndMainThreadStabilityMonitor() =
    default;

void NetworkAndMainThreadStabilityMonitor::WaitForStable(
    base::OnceClosure callback) {
  CHECK(!is_stable_callback_);
  is_stable_callback_ = std::move(callback);

  size_t after_request_count =
      render_frame_->GetWebFrame()->GetDocument().ActiveResourceRequestCount();
  journal_->Log(task_id_, "NetworkAndMainThreadStabilityMonitor: WaitForStable",
                JournalDetailsBuilder()
                    .Add("requests_after", after_request_count)
                    .Build());

  if (after_request_count > starting_request_count_) {
    render_frame_->GetWebFrame()->RequestNetworkIdleCallback(
        base::BindOnce(&NetworkAndMainThreadStabilityMonitor::OnNetworkIdle,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    WaitForMainThreadIdle();
  }
}

void NetworkAndMainThreadStabilityMonitor::OnNetworkIdle() {
  journal_->Log(task_id_, "NetworkAndMainThreadStabilityMonitor::OnNetworkIdle",
                {});
  WaitForMainThreadIdle();
}

void NetworkAndMainThreadStabilityMonitor::WaitForMainThreadIdle() {
  render_frame_->GetWebFrame()->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle(base::TimeTicks) {
  journal_->Log(task_id_,
                "NetworkAndMainThreadStabilityMonitor::OnMainThreadIdle", {});

  CHECK(is_stable_callback_);
  std::move(is_stable_callback_).Run();
}

}  // namespace actor
