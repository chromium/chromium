// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SECURITY_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SECURITY_SETTINGS_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace settings {

// Add security-specific load-time data to `html_source`.
void AddSecurityData(content::WebUIDataSource* html_source);

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SECURITY_SETTINGS_PROVIDER_H_
