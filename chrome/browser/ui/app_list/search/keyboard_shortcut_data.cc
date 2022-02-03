// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_data.h"

namespace app_list {

// TODO(crbug.com/1290682): Continue implementation.
KeyboardShortcutData::KeyboardShortcutData(
    const ash::KeyboardShortcutItem& item)
    : description_message(
          l10n_util::GetStringUTF16(item.description_message_id)) {}

KeyboardShortcutData::~KeyboardShortcutData() = default;

}  // namespace app_list
