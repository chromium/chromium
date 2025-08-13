// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_

inline constexpr char kSignInStateKey[] = "signInState";

// This enum is used to differentiate all the relevant sign-in/history-sync
// states.
// LINT.IfChange(HistorySignInState)
enum class HistorySignInState {
  kSignedOut = 0,
  // TODO(crbug.com/418144047): Add additional signin states (like signed in
  // without history). Also rename kSignedIn to better reflect what it actually
  // means - currently it means "Sync-the-feature is enabled".
  kSignedIn = 1,
};
// LINT.ThenChange(/chrome/browser/resources/history/constants.ts:HistorySignInState)

class Profile;

namespace content {
class WebUIDataSource;
}

class HistoryUtil {
 public:
  static HistorySignInState GetSignInState(Profile* profile);
  static content::WebUIDataSource* PopulateSourceForSidePanelHistory(
      content::WebUIDataSource* source,
      Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_HISTORY_HISTORY_UTIL_H_
