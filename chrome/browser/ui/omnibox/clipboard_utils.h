// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions for accessing the clipboard.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CLIPBOARD_UTILS_H_
#define CHROME_BROWSER_UI_OMNIBOX_CLIPBOARD_UTILS_H_

#include <string>


// Truncates the clipboard text returned in order to improve performance and
// prevent unresponsiveness. For reference, a book is about ~500k characters and
// data URLs served by google images are usually 30k characters or less.
// We don't use url::kMaxURLChars (2M), as it's too large; it adds 2s+ delays
// when right clicking the omnibox for clipboards larger than 2M. Additionally,
// a 500k limit also allows us to not have to worry about length when
// classifying the text: OmniboxViewViews::GetLabelForCommandId and
// OmniboxEditModel::CanPasteAndGo. If we used a larger limit here (e.g. 2M),
// then we'd need to limit the text to ~500k later anyways when classifying,
// because classifying text longer than 500k adds a ~1s delays.
static const size_t kMaxClipboardTextLength = 500 * 1024;

// Returns the current clipboard contents as a string that can be pasted in.
// In addition to just getting CF_UNICODETEXT out, this can also extract URLs
// from bookmarks on the clipboard.
// If `notify_if_restricted` is set to true, a notification will be shown to
// the user if the clipboard contents can't be accessed.
std::u16string GetClipboardText(bool notify_if_restricted);

#endif  // CHROME_BROWSER_UI_OMNIBOX_CLIPBOARD_UTILS_H_
