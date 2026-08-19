// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CROSS_TAB_COPY_PASTE_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CROSS_TAB_COPY_PASTE_TRACKER_H_

#include <optional>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

// Tracks cross-tab copy-paste events.
class CrossTabCopyPasteTracker : public KeyedService {
 public:
  CrossTabCopyPasteTracker();
  CrossTabCopyPasteTracker(const CrossTabCopyPasteTracker&) = delete;
  CrossTabCopyPasteTracker& operator=(const CrossTabCopyPasteTracker&) = delete;
  ~CrossTabCopyPasteTracker() override;

  // KeyedService:
  void Shutdown() override;

  // Records a copy action.
  void OnCopy(SessionID tab_id, size_t content_hash, ukm::SourceId source_id);

  // Records a paste action and returns true if it completes a valid sequence
  // (Paste in a different tab than the copy, within the time limit).
  bool OnPaste(SessionID tab_id, ukm::SourceId source_id);

 private:
  // Stores metadata about a clipboard copy event, which is used to correlate
  // subsequent paste events and perform the UKM metrics logging.
  struct ActionRecord {
    // The timestamp of the copy event.
    base::TimeTicks time;
    // The hash of the copied text.
    size_t hash = 0;
    // A random identifier used to identify the copy event.
    int64_t nonce = 0;
    // The ID of the tab where the copy event occurred.
    SessionID tab_id;
  };

  // Callback fired whenever text is read from the clipboard during a paste
  // operation. If the text hashes match, it records the `PasteFromTabNonce`
  // UKM metric.
  static void OnClipboardTextRead(ActionRecord last_copy,
                                  ukm::SourceId source_id,
                                  std::u16string text);

  std::optional<ActionRecord> last_copy_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_CROSS_TAB_COPY_PASTE_TRACKER_H_
