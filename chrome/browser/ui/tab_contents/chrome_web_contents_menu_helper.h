// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_MENU_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_MENU_HELPER_H_

namespace content {
class WebContents;
struct ContextMenuParams;
}  // namespace content

// Add properties that are specified via preferences and influence the context
// menu.
content::ContextMenuParams AddContextMenuParamsPropertiesFromPreferences(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params);

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CHROME_WEB_CONTENTS_MENU_HELPER_H_
