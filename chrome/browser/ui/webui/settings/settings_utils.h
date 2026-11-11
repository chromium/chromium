// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UTILS_H_

#include <string>

#include "build/build_config.h"
#include "ui/base/interaction/element_tracker.h"

class GURL;
class PrefService;
class Profile;

namespace content {
class WebContents;
class WebUI;
}

// Chrome settings utility methods.
namespace settings_utils {

// Returns whether Settings treats `profile` as a guest. On ChromeOS this is a
// property of the whole session and includes managed guest sessions.
bool IsGuest(const Profile* profile);

// Returns whether Settings shows the Security keys subpage, including its
// bio enrollment controls. On Windows, when device::kWebAuthUseNativeWinApi is
// enabled and the native WebAuthn API is available, Windows handles security
// keys instead.
bool IsSecurityKeysSubpageEnabled();

// Invoke UI for network proxy settings.
void ShowNetworkProxySettings(content::WebContents* web_contents);

// Invoke UI for SSL certificates.
void ShowManageSSLCertificates(content::WebContents* web_contents);

// Returns whether |url_string| is a valid startup page. |fixed_url| is set to
// the fixed up, valid URL if not null.
bool FixupAndValidateStartupPage(const std::string& url_string,
                                 GURL* fixed_url);

// Dispatches `event_type` on the owning browser window's
// `kBrowserViewElementId` for Settings Critical User Journey (CUJ) telemetry.
// Resolving the window via `webui::GetBrowserWindowInterface` lets custom
// events emitted from both `chrome://settings` tabs and Side Panel WebUIs share
// a per-window `ElementContext` while keeping multi-window sessions isolated.
// No-ops when `metrics::kSettingsGlowupJourneys` is disabled or when
// `web_contents` has no associated `BrowserWindowInterface` (e.g., in unit
// tests using a detached `content::TestWebUI`).
void MaybeNotifySettingsCustomEvent(content::WebContents* web_contents,
                                    ui::CustomElementEventType event_type);
void MaybeNotifySettingsCustomEvent(content::WebUI* web_ui,
                                    ui::CustomElementEventType event_type);

#if BUILDFLAG(IS_MAC)
void ValidateSavedFonts(PrefService* prefs);
#endif

}  // namespace settings_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SETTINGS_UTILS_H_
