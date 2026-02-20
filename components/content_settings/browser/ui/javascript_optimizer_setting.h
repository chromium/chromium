// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_JAVASCRIPT_OPTIMIZER_SETTING_H_
#define COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_JAVASCRIPT_OPTIMIZER_SETTING_H_

namespace content_settings {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Must be kept in sync with the JavascriptOptimizerSetting enum in
// chrome/browser/resources/settings/site_settings/constants.ts
// LINT.IfChange(JavascriptOptimizerSetting)
enum class JavascriptOptimizerSetting {
  kBlocked = 0,
  kAllowed = 1,
  kBlockedForUnfamiliarSites = 2,
  kMaxValue = kBlockedForUnfamiliarSites,
};
// LINT.ThenChange(//chrome/browser/resources/settings/site_settings/constants.ts:JavascriptOptimizerSetting)

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_BROWSER_UI_JAVASCRIPT_OPTIMIZER_SETTING_H_
