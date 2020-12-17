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
]);

GEN('#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"');
GEN('#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "components/prefs/pref_service.h"');

// eslint-disable-next-line no-var
var CrostiniAccessibilityTest = class extends Polymer2DeprecatedTest {
  /** @override */
  get featureList() {
    return {enabled: ['features::kCrostini']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  testGenPreamble() {
    GEN('  browser()->profile()->GetPrefs()->SetBoolean(');
    GEN('      crostini::prefs::kCrostiniEnabled, true);');
    GEN('  crostini::FakeCrostiniFeatures fake_crostini_features;');
    GEN('  fake_crostini_features.SetAll(true);');
  }
};
