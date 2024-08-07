// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/content/browser/last_replaced_clipboard_data.h"

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_util.h"

namespace data_controls {

LastReplacedClipboardData& GetLastReplacedClipboardData() {
  static base::NoDestructor<LastReplacedClipboardData> data;
  return *data.get();
}

// static
LastReplacedClipboardDataObserver*
LastReplacedClipboardDataObserver::GetInstance() {
  static base::NoDestructor<LastReplacedClipboardDataObserver> observer;
  return observer.get();
}

void LastReplacedClipboardDataObserver::AddDataToNextSeqno(
    content::ClipboardPasteData data) {
  // Bitmap isn't used directly by pasting code, so it must first be converted
  // to PNG before being stored in `GetLastReplacedClipboardData()`.
  if (!data.bitmap.empty()) {
    data.png =
        ui::clipboard_util::EncodeBitmapToPngAcceptJank(std::move(data.bitmap));
    data.bitmap = SkBitmap();
  }

  if (pending_seqno_data_.empty()) {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }
  pending_seqno_data_.Merge(std::move(data));
}

void LastReplacedClipboardDataObserver::OnClipboardDataChanged() {
  GetLastReplacedClipboardData() = {
      .seqno = ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
      // `LastReplacedClipboardData::clipboard_paste_data` is reassigned to
      // clear previous data corresponding to an older seqno.
      .clipboard_paste_data = std::move(pending_seqno_data_),
  };

  // Explicitly clear `pending_seqno_data_` in case it eventually holds a data
  // member that doesn't clear itself cleanly after moving.
  pending_seqno_data_ = content::ClipboardPasteData();

  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

}  // namespace data_controls
