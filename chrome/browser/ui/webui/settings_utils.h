// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_

#include <string>

#include "base/macros.h"
#include "ui/base/resource/scale_factor.h"

class GURL;
class PrefService;

namespace base {
class RefCountedMemory;
}

namespace content {
class WebContents;
}

// Chrome settings utility methods.
namespace settings_utils {

// Invoke UI for network proxy settings.
void ShowNetworkProxySettings(content::WebContents* web_contents);

// Invoke UI for SSL certificates.
void ShowManageSSLCertificates(content::WebContents* web_contents);

// Returns whether |url_string| is a valid startup page. |fixed_url| is set to
// the fixed up, valid URL if not null.
bool FixupAndValidateStartupPage(const std::string& url_string,
                                 GURL* fixed_url);

base::RefCountedMemory* GetFaviconResourceBytes(ui::ScaleFactor scale_factor);

#if defined(OS_MACOSX)
void ValidateSavedFonts(PrefService* prefs);
#endif

// When |font_name_or_list| starts with ",", it is a list of font names
// separated by "," and this function returns the first available font name.
// Otherwise returns |font_name_or_list| as is.
// Unlike gfx::FontList, this function picks one font, and character-level
// fallback is handled in CSS.
std::string ResolveFontList(const std::string& font_name_or_list);

}  // namespace settings_utils

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_UTILS_H_
