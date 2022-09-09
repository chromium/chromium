// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_

namespace content {
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace send_tab_to_self {

void ShowBubble(content::WebContents* web_contents,
                bool show_back_button = false);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* user_prefs);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_H_
