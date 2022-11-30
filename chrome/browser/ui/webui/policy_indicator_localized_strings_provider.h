// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_INDICATOR_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_INDICATOR_LOCALIZED_STRINGS_PROVIDER_H_

namespace content {
class WebUIDataSource;
}

namespace policy_indicator {

// Adds the strings needed for policy indicators to |html_source|. This function
// causes |html_source| to expose a strings.js file from its source which
// contains a mapping from string's name to its translated value.
void AddLocalizedStrings(content::WebUIDataSource* html_source);

}  // namespace policy_indicator

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_INDICATOR_LOCALIZED_STRINGS_PROVIDER_H_
