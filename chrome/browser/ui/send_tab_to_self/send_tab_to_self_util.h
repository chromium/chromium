// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include "base/memory/weak_ptr.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

}  // namespace send_tab_to_self

class Profile;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

// Opens the given `entry` in a new foreground tab for the given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewForegroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry);

// Opens the given `entry` in a new background tab for the given `profile`.
// Returns a weak pointer to the opened WebContents.
base::WeakPtr<content::WebContents> OpenEntryInNewBackgroundTab(
    Profile* profile,
    const SendTabToSelfEntry& entry);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
