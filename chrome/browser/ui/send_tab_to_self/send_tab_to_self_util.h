// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace send_tab_to_self {

class SendTabToSelfEntry;

}  // namespace send_tab_to_self

class Profile;

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

// Shows a success toast confirming that the tab was successfully sent, if
// `kSendTabToSelfPostSendToast` is enabled.
void ShowTabSentSuccessToast(content::WebContents* web_contents,
                             std::string_view device_name);

// Shows a toast confirming that the tab was already sent to the device
// recently, if `kSendTabToSelfPostSendToast` is enabled.
void ShowTabSentThrottledToast(content::WebContents* web_contents,
                               std::string_view device_name);

// Shows a failure toast (or notification if the feature flag is disabled)
// when the tab failed to send.
void ShowTabSentFailure(content::WebContents* web_contents,
                        const GURL& url = GURL());

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_UTIL_H_
