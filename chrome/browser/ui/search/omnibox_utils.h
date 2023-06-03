// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_

class Browser;
class OmniboxView;
namespace content {
class WebContents;
}  // namespace content

namespace search {

// Returns the omnibox view from the browser instance associated with
// `web_contents`, if any, or nullptr otherwise.
OmniboxView* GetOmniboxView(content::WebContents* web_contents);
// Returns the omnibox view from `browser`, if provided, or nullptr otherwise.
OmniboxView* GetOmniboxView(Browser* browser);
// Focus or unfocus the omnibox if |focus| is true or false respectively.
void FocusOmnibox(bool focus, content::WebContents* web_contents);
// Returns whether input is in progress, i.e. if the omnibox has focus and the
// active tab is in mode SEARCH_SUGGESTIONS.
bool IsOmniboxInputInProgress(content::WebContents* web_contents);

}  // namespace search

#endif  // CHROME_BROWSER_UI_SEARCH_OMNIBOX_UTILS_H_
