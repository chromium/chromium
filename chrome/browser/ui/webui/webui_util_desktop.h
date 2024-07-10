// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_DESKTOP_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_DESKTOP_H_

#include <string>

namespace content {
class WebContents;
}

namespace ui {
class NativeTheme;
class ThemeProvider;
}  // namespace ui

class GURL;

namespace webui {

// These methods should not be used. Instead, browser-related state should be
// passed from the owner of the WebUI instance to the WebUIController instance.
// See LensOverlayController for one example of how to do this.
// Returns whether WebContents should use dark mode colors depending on the
// theme.
ui::NativeTheme* GetNativeThemeDeprecated(content::WebContents* web_contents);

// Returns the ThemeProvider instance associated with the given web contents.
const ui::ThemeProvider* GetThemeProviderDeprecated(
    content::WebContents* web_contents);

// Sets a global theme provider that will be returned when calling
// webui::GetThemeProviderDeprecated(). Used only for testing.
void SetThemeProviderForTestingDeprecated(
    const ui::ThemeProvider* theme_provider);

// Gets the metrics appropriate hostname for a given WebUI URL for code cache
// metrics. Returns an empty string if no relevant mapping has been defined.
std::string GetWebUIHostnameForCodeCacheMetrics(const GURL& webui_url);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_DESKTOP_H_
