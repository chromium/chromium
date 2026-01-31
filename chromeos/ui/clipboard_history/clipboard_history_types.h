// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_TYPES_H_
#define CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_TYPES_H_

#include <string>

#include "base/unguessable_token.h"

namespace chromeos::clipboard_history {

// The different ways the multipaste menu can be shown. These values are
// written to logs. New enum values can be added, but existing enums must
// never be renumbered, deleted, or reused. If adding an enum, add it at the
// bottom.
enum class ShowSource {
  // Shown by the accelerator(search + v).
  kAccelerator = 0,
  // Shown by a render view's context menu.
  kRenderViewContextMenu = 1,
  // Shown by a textfield context menu.
  kTextfieldContextMenu = 2,
  // Shown by the virtual keyboard.
  kVirtualKeyboard = 3,
  // Deprecated: Used as default value in case of version skew.
  kUnknown = 4,
  // Deprecated: Shown by a toast.
  kToast = 5,
  // Deprecated: Shown by long-pressing Ctrl+V.
  kControlVLongpress = 6,
  // Shown from the submenu embedded in a render view's context memu.
  kRenderViewContextSubmenu = 7,
  // Shown from the submenu embedded in a textfield context menu.
  kTextfieldContextSubmenu = 8,
  kMinValue = kAccelerator,
  kMaxValue = kTextfieldContextSubmenu,
};

// The formats dictating how clipboard history items are displayed.
// Maps to the `ClipboardHistoryDisplayFormat` enum used in histograms. Do not
// reorder entries; append any new ones to the end.
enum class DisplayFormat {
  kUnknown = -1,
  kText = 0,
  kPng = 1,
  kHtml = 2,
  kFile = 3,
  kMinValue = kUnknown,
  kMaxValue = kFile,
};

// Describes a clipboard history item.
//
// NOTE: This structure does not contain the actual clipboard data. The
// clipboard data is indicated by the `display_text` and `display_format`
// fields.
struct ItemDescriptor {
  ItemDescriptor(const base::UnguessableToken& item_id,
                 DisplayFormat display_format,
                 std::u16string display_text,
                 size_t file_count)
      : item_id(item_id),
        display_format(display_format),
        display_text(std::move(display_text)),
        file_count(file_count) {}

  // The unique identifier for the clipboard history item.
  base::UnguessableToken item_id;

  // The format of the clipboard data.
  DisplayFormat display_format;

  // The text that is displayed in the clipboard history menu.
  std::u16string display_text;

  // The number of files in the corresponding clipboard history item.
  size_t file_count;
};

}  // namespace chromeos::clipboard_history

#endif  // CHROMEOS_UI_CLIPBOARD_HISTORY_CLIPBOARD_HISTORY_TYPES_H_
