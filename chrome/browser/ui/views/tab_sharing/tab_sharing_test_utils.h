// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_TEST_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_TEST_UTILS_H_

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_infobar.h"

// Calls `GetText()` on the passed in `button_or_label` which must be of type
// `MdTextButton` or `Label`.
std::optional<std::u16string_view> GetButtonOrLabelText(
    const views::View& button_or_label);

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_TEST_UTILS_H_
