// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

namespace send_tab_to_self {

class SendTabToSelfEntry;

}  // namespace send_tab_to_self

class Profile;

namespace send_tab_to_self {

// Opens the given `entry` in a new foreground tab for the given `profile`.
void OpenEntryInNewForegroundTab(Profile* profile,
                                 const SendTabToSelfEntry& entry);

// Opens the given `entry` in a new background tab for the given `profile`.
void OpenEntryInNewBackgroundTab(Profile* profile,
                                 const SendTabToSelfEntry& entry);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
