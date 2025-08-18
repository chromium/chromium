// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_

inline constexpr char kSignInStateKey[] = "signInState";

class Profile;

namespace content {
class WebUIDataSource;
}

class HistoryUtil {
 public:
  // Populates `source` with strings and configuration data that's used by both
  // the main chrome://history UI and the history side panel.
  static content::WebUIDataSource* PopulateCommonSourceForHistory(
      content::WebUIDataSource* source,
      Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_
