// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Runs the Polymer Accessibility Settings tests.
 *  Chrome OS only.
 */

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  '//chrome/test/data/webui/a11y/accessibility_test.js',
  '//chrome/test/data/webui/polymer_browser_test_base.js',
  '//chrome/test/data/webui/settings/a11y/settings_accessibility_test.js',
]);

GEN('#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "components/prefs/pref_service.h"');

// eslint-disable-next-line no-var
var CrostiniAccessibilityTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    // Always test with SplitSettings on because the pages are the same in the
    // legacy combined settings and we don't want to test everything twice.
    return {
      enabled: ['features::kCrostini', 'chromeos::features::kSplitSettings']
    };
  }

  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  testGenPreamble() {
    GEN('  browser()->profile()->GetPrefs()->SetBoolean(');
    GEN('      crostini::prefs::kCrostiniEnabled, true);');
  }
};
