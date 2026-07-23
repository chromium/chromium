// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_

#include "content/public/browser/clipboard_types.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"

namespace data_controls {

// Represents the enforcement state of a clipboard copy operation tracked inside
// `LastReplacedClipboardData`. When pasting across tab boundaries or evaluating
// context menus, these values determine whether pasting is allowed, blocked,
// or restricted to managed browser contexts.
enum class CopyRestrictionLevel {
  // Default uninitialized state, or data whose copy was completely
  // unrestricted.
  kNone = 0,
  // Indicates an asynchronous scan is currently in progress; cross-tab pastes
  // are blocked while the OS clipboard holds a temporary scanning placeholder.
  kOngoingScan = 1,
  // Indicates copy is permitted inside managed Chrome contexts, but external OS
  // apps are blocked.
  kKeptInManagedChrome = 2,
  // Indicates the copy was strictly prohibited by policy or scanning
  // evaluation. All pastes are strictly blocked due to the OS clipboard holding
  // a warning string.
  kBlocked = 3,
};

// Struct that holds information on the last data to have been replaced in the
// OS clipboard by a Data Controls rule.
struct LastReplacedClipboardData {
  ui::ClipboardSequenceNumberToken seqno;
  content::ClipboardPasteData clipboard_paste_data;
  CopyRestrictionLevel restriction_level = CopyRestrictionLevel::kNone;

  std::vector<std::u16string> GetAvailableTypes() const;
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
  // `restriction_level` is the policy restriction level that resulted in
  // `data` being replaced.
  void AddDataToNextSeqno(content::ClipboardPasteData data,
                          CopyRestrictionLevel restriction_level);

  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override;

 private:
  // Data recently copied from Chrome, waiting to be tied to a sequence number.
  content::ClipboardPasteData pending_seqno_data_;
  CopyRestrictionLevel pending_restriction_level_ = CopyRestrictionLevel::kNone;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CONTENT_BROWSER_LAST_REPLACED_CLIPBOARD_DATA_H_
