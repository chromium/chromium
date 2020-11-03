// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Nearby Share WebUI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer Nearby Share elements. */
const NearbyBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  /**
   * Override default |extraLibraries| since PolymerTest includes more than are
   * needed in JS Module based tests.
   * @override
   */
  get extraLibraries() {
    return [
      '//third_party/mocha/mocha.js',
      '//chrome/test/data/webui/mocha_adapter.js',
    ];
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
 ['Device', 'nearby_device_test.js'],
 ['DeviceIcon', 'nearby_device_icon_test.js'],
 ['DiscoveryPage', 'nearby_discovery_page_test.js'],
 ['Preview', 'nearby_preview_test.js'],
 ['Progress', 'nearby_progress_test.js'],
 ['ShareApp', 'nearby_share_app_test.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `Nearby${testName}Test`;
  this[className] = class extends NearbyBrowserTest {
    /** @override */
    get browsePreload() {
      return `chrome://nearby/test_loader.html?module=nearby_share/${module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
