// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_handle.h"

namespace send_tab_to_self {

class SendTabToSelfEntry;

}  // namespace send_tab_to_self

struct NavigateParams;
class Profile;

namespace send_tab_to_self {

// Opens the given `entry` in a new foreground tab for the given `profile`.
void OpenEntryInNewTab(Profile* profile, const SendTabToSelfEntry& entry);

// Opens the given `entry` in a new foreground tab for the given `profile`,
// using the provided `navigate_callback` to perform the navigation.
void OpenEntryInNewTabWithNavigationCallback(
    Profile* profile,
    const SendTabToSelfEntry& entry,
    base::OnceCallback<base::WeakPtr<content::NavigationHandle>(
        NavigateParams*)> navigate_callback);

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
