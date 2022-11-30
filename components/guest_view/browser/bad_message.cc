// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/render_process_host.h"

namespace guest_view {
namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  LOG(ERROR) << "Terminating renderer for bad IPC message, reason " << reason;
  base::UmaHistogramSparse("Stability.BadMessageTerminated.GuestView", reason);
}

}  // namespace

void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  LogBadMessage(reason);
  host->ShutdownForBadMessage(
      content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

void ReceivedBadMessage(int render_process_id, BadMessageReason reason) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)
    return;

  ReceivedBadMessage(rph, reason);
}

}  // namespace bad_message
}  // namespace guest_view
