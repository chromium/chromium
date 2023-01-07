// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderFrameHost;
}  // namespace content

namespace autofill {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum class BadMessageReason {
  // Prerendered frames should not attempt to interact with browser-side
  // autofill code if they are well behaved. If we receive such a message, we
  // terminate the renderer to avoid it sticking around and causing issues. This
  // will also let us confirm that there are no existing or future code paths
  // that unexpectedly start sending these messages.
  kPrerendering = 1,

  // Please add new elements here. After making changes, you MUST update
  // histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  kMaxValue = kPrerendering
};

namespace bad_message {

// Returns true if frame is not prerendering (when autofill updates are
// disallowed). Kills the renderer if we are prerendering.
bool CheckFrameNotPrerendering(content::RenderFrameHost* frame);

}  // namespace bad_message
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_BAD_MESSAGE_H_
