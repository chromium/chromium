// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Nearby Share WebUI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer Nearby Share elements. */
const NearbyBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'nearby';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kNearbySharing']};
  }
};

[['ConfirmationPage', 'nearby_confirmation_page_test.js'],
 ['DiscoveryPage', 'nearby_discovery_page_test.js'],
 ['ShareApp', 'nearby_share_app_test.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `Nearby${testName}Test`;
  this[className] = class extends NearbyBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://nearby/test_loader.html?module=chromeos/nearby_share/${
          module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
