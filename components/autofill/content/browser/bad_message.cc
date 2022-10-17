// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace autofill {
namespace bad_message {
namespace {

// Called when the browser receives a bad IPC message from a renderer process on
// the UI thread. Logs the event, records a histogram metric for the |reason|,
// and terminates the process for |host|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  LOG(ERROR)
      << "Terminating renderer for bad AutofillManager IPC message, reason "
      << static_cast<int>(reason);
  base::UmaHistogramSparse("Stability.BadMessageTerminated.Autofill",
                           static_cast<int>(reason));
  host->ShutdownForBadMessage(
      content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

}  // namespace

bool CheckFrameNotPrerendering(content::RenderFrameHost* frame) {
  if (frame->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    ReceivedBadMessage(frame->GetProcess(), BadMessageReason::kPrerendering);
    return false;
  }
  return true;
}

}  // namespace bad_message
}  // namespace autofill
