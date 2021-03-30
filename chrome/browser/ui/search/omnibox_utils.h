// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_

#include <string>


namespace content {
class WebContents;
}  // namespace content

namespace search {

// Focus or unfocus the omnibox if |focus| is true or false respectively.
void FocusOmnibox(bool focus, content::WebContents* web_contents);
// Pastes |text| (or the clipboard if |text| is empty) into the omnibox.
void PasteIntoOmnibox(const std::u16string& text,
                      content::WebContents* web_contents);
// Returns whether input is in progress, i.e. if the omnibox has focus and the
// active tab is in mode SEARCH_SUGGESTIONS.
bool IsOmniboxInputInProgress(content::WebContents* web_contents);

}  // namespace search

#endif  // CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
