// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/cross_tab_copy_paste_tracker.h"

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/clipboard/clipboard.h"

namespace autofill {

namespace {

// Maximum duration between copy and paste in a valid sequence for UKM logging.
constexpr base::TimeDelta kUkmLoggingSequenceDuration = base::Minutes(10);

}  // namespace

CrossTabCopyPasteTracker::CrossTabCopyPasteTracker() = default;

CrossTabCopyPasteTracker::~CrossTabCopyPasteTracker() = default;

// static
void CrossTabCopyPasteTracker::OnClipboardTextRead(ActionRecord last_copy,
                                                   ukm::SourceId source_id,
                                                   std::u16string text) {
  // If the paste is within the time window, is from a different tab, and the
  // content hash matches then log the UKM paste event.
  if (base::FastHash(base::as_byte_span(text)) != last_copy.hash) {
    return;
  }
  ukm::builders::Clipboard_CopyPasteEvent(source_id)
      .SetPasteFromTabNonce(last_copy.nonce)
      .Record(ukm::UkmRecorder::Get());
}

void CrossTabCopyPasteTracker::Shutdown() {
  last_copy_.reset();
}

void CrossTabCopyPasteTracker::OnCopy(SessionID tab_id,
                                      size_t content_hash,
                                      ukm::SourceId source_id) {
  last_copy_.emplace(base::TimeTicks::Now(), content_hash,
                     static_cast<int64_t>(base::RandUint64()), tab_id);
  ukm::builders::Clipboard_CopyPasteEvent(source_id)
      .SetCopyFromTabNonce(last_copy_->nonce)
      .Record(ukm::UkmRecorder::Get());
}

void CrossTabCopyPasteTracker::OnPaste(SessionID tab_id,
                                       ukm::SourceId source_id) {
  if (!last_copy_.has_value()) {
    return;
  }

  const bool is_different_tab = last_copy_->tab_id != tab_id;
  if (!is_different_tab) {
    return;
  }
  const bool is_recent_for_ukm = (base::TimeTicks::Now() - last_copy_->time) <=
                                 kUkmLoggingSequenceDuration;
  if (is_recent_for_ukm) {
    ui::Clipboard::GetForCurrentThread()->ReadText(
        ui::ClipboardBuffer::kCopyPaste,
        ui::DataTransferEndpoint(ui::EndpointType::kDefault,
                                 {.notify_if_restricted = false}),
        base::BindOnce(&CrossTabCopyPasteTracker::OnClipboardTextRead,
                       *last_copy_, source_id));
  }
  last_copy_.reset();
}

}  // namespace autofill
