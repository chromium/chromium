// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_SITE_SETTINGS_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_SITE_SETTINGS_ANDROID_H_

#include <string>

namespace content {
class WebContents;
}

namespace extensions {

// Shows the site settings page for the extension with `extension_id`, i.e. the
// permissions held by the pages served from chrome-extension://<extension_id>.
//
// Settings is a native page hosted in a tab on desktop Android, so the page is
// opened in a new tab next to `web_contents` by its URL, the same way as on
// desktop. A null `web_contents`, or a configuration in which a settings tab
// cannot resolve a URL to a page, falls back to naming the page over the Java
// bridge, which shows it in a settings activity of its own.
void ShowExtensionSiteSettings(content::WebContents* web_contents,
                               const std::string& extension_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_SITE_SETTINGS_ANDROID_H_
