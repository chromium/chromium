// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_

#include "content/public/browser/clipboard_types.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"

namespace data_controls {

// Struct that holds information on the last data to have been replaced in the
// OS clipboard by a Data Controls rule.
struct LastReplacedClipboardData {
  ui::ClipboardSequenceNumberToken seqno;
  content::ClipboardPasteData clipboard_paste_data;
};

// Get the last data to have been replaced by a warning string due to a Data
// Controls rule.
LastReplacedClipboardData& GetLastReplacedClipboardData();

// Clipboard change observer used to observe seqno changes and update the data
// in `GetLastReplacedClipboardData()`.
class LastReplacedClipboardDataObserver : public ui::ClipboardObserver {
 public:
  static LastReplacedClipboardDataObserver* GetInstance();

  // Adds `data` to `pending_seqno_data_` so that it can be associated to the
  // next sequence number change. Note that because this can be called multiple
  // times with different data types (text, html, png, etc.) before
  // `OnClipboardDataChanged()` is called, `data` is merged into
  // `pending_seqno_data_` instead of replacing it entirely.
  void AddDataToNextSeqno(content::ClipboardPasteData data);

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

 private:
  // Data recently copied from Chrome, waiting to be tied to a sequence number.
  content::ClipboardPasteData pending_seqno_data_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_
