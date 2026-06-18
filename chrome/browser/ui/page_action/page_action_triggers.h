// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_

#include "ui/base/class_property.h"

namespace page_actions {
enum class PageActionTrigger {
  kMouse = 0,
  kKeyboard = 1,
  kGesture = 2,
};

constexpr std::underlying_type_t<page_actions::PageActionTrigger>
    kInvalidPageActionTrigger = -1;

enum class PageActionEntryPoint {
  kSuggestionChip = 0,
  kAnchoredMessage = 1,
};

constexpr std::underlying_type_t<page_actions::PageActionEntryPoint>
    kInvalidPageActionEntryPoint = -1;

extern const ui::ClassProperty<std::underlying_type_t<PageActionTrigger>>* const
    kPageActionTriggerKey;
extern const ui::ClassProperty<
    std::underlying_type_t<PageActionEntryPoint>>* const
    kPageActionEntryPointKey;

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_TRIGGERS_H_
