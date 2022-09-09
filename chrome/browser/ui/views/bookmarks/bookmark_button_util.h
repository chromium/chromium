// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_UTIL_H_

#include <memory>

namespace views {
class LabelButtonBorder;
}

namespace bookmark_button_util {

// Max width of the buttons in the bookmark bar.
constexpr int kMaxButtonWidth = 150;

std::unique_ptr<views::LabelButtonBorder> CreateBookmarkButtonBorder();

}  // namespace bookmark_button_util

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_BUTTON_UTIL_H_
