// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_provider.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

KeyboardShortcutProvider::KeyboardShortcutProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

KeyboardShortcutProvider::~KeyboardShortcutProvider() = default;

void KeyboardShortcutProvider::Start(const std::u16string& query) {
  // TODO(crbug.com/1290682): Implement.
}

ash::AppListSearchResultType KeyboardShortcutProvider::ResultType() const {
  return ash::AppListSearchResultType::kKeyboardShortcut;
}

}  // namespace app_list
