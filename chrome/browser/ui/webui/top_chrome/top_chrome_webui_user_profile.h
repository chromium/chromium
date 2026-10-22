// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_USER_PROFILE_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_USER_PROFILE_H_

class Profile;

namespace content {
class WebContents;
}  // namespace content

// TopChromeWebUIProfile, which exists only when
// features::kTopChromeWebUISharedRenderer is enabled, is shared by the Top
// Chrome WebUIs of the windows of all profiles. These functions associate a
// WebContents created in it with the user profile it is used for, i.e. the
// profile of the window that shows it.
namespace webui {

// Sets the user profile for `web_contents` created in TopChromeWebUIProfile.
// The client that creates `web_contents` must call this right after creating
// it, before its WebPreferences are first computed, and only once.
void SetUserProfile(content::WebContents* web_contents, Profile* user_profile);

// Returns the user profile for `web_contents`. For `web_contents` created in
// TopChromeWebUIProfile, this is the profile set by SetUserProfile(), or null
// if it was not set or has been destroyed. Otherwise, this is the profile of
// `web_contents`.
Profile* GetUserProfile(content::WebContents* web_contents);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_USER_PROFILE_H_
