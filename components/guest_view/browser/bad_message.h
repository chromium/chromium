// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderProcessHost;
}  // namespace content

namespace guest_view {
namespace bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum BadMessageReason {
  GVM_EMBEDDER_FORBIDDEN_ACCESS_TO_GUEST = 0,
  GVM_INVALID_GUESTVIEW_TYPE = 1,
  GVMF_UNEXPECTED_MESSAGE_BEFORE_GVM_CREATION = 2,
  GVM_INVALID_ATTACH = 3,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. GuestViewManager becomes GVM) plus a unique description of
  // the reason. After making changes, you MUST update histograms.xml by
  // running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from a renderer.
// Logs the event, records a histogram metric for the |reason|,
// and terminates the process for |host| / |render_process_id|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason);
void ReceivedBadMessage(int render_process_id, BadMessageReason reason);

}  // namespace bad_message
}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_BAD_MESSAGE_H_
