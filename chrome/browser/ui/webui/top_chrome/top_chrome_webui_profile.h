// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_PROFILE_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_PROFILE_H_

class GURL;
class Profile;

// The process-wide profile that hosts the Top Chrome WebUIs of the windows of
// all profiles if ShouldUseTopChromeWebUIProfile(). It is not managed by
// ProfileManager. See TopChromeWebUIProfileImpl.
namespace webui {

// Returns whether the Top Chrome WebUIs in the list of
// GetProfileForTopChromeWebUIContents() are created in the profile:
// features::kTopChromeWebUISharedRenderer is enabled, and renderers don't run
// in the browser process (--single-process), where the only renderer process
// can't host the WebUIs of another BrowserContext.
bool ShouldUseTopChromeWebUIProfile();

// Creates the profile. Called once at startup, after the initial user profile
// is loaded and before any Top Chrome WebUI is created.
void CreateTopChromeWebUIProfile();

// Destroys the profile. Called after the user profiles are destroyed, since
// their services may own WebContents created in this profile. Does nothing if
// the profile was not created.
void DestroyTopChromeWebUIProfile();

// Returns the profile, or null if it has not been created or has been
// destroyed.
Profile* GetTopChromeWebUIProfile();

// Returns the profile to create the WebContents of the Top Chrome WebUI at
// `url` in for a window of `user_profile`: this profile if
// ShouldUseTopChromeWebUIProfile() and the WebUI has moved to this profile, or
// `user_profile` otherwise.
Profile& GetProfileForTopChromeWebUIContents(Profile* user_profile,
                                             const GURL& url);

// Creates the profile for the lifetime of this object.
class ScopedTopChromeWebUIProfileForTesting {
 public:
  ScopedTopChromeWebUIProfileForTesting();
  ScopedTopChromeWebUIProfileForTesting(
      const ScopedTopChromeWebUIProfileForTesting&) = delete;
  ScopedTopChromeWebUIProfileForTesting& operator=(
      const ScopedTopChromeWebUIProfileForTesting&) = delete;
  ~ScopedTopChromeWebUIProfileForTesting();
};

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_TOP_CHROME_WEBUI_PROFILE_H_
