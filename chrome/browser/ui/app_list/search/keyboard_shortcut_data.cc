// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_data.h"

#include "base/strings/string_util.h"

namespace app_list {

// TODO(crbug.com/1290682): Continue implementation.
KeyboardShortcutData::KeyboardShortcutData(
    const ash::KeyboardShortcutItem& item)
    : description_message_id(item.description_message_id),
      description(base::CollapseWhitespace(
          l10n_util::GetStringUTF16(item.description_message_id),
          true)),
      shortcut_message_id(item.shortcut_message_id),
      shortcut_key_codes(item.shortcut_key_codes) {}

KeyboardShortcutData::KeyboardShortcutData(const KeyboardShortcutData&) =
    default;

KeyboardShortcutData::~KeyboardShortcutData() = default;

}  // namespace app_list
