// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer v3 Nearby Share shared tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer Nearby Share Shared elements. */
const NearbySharedV3Test = class extends PolymerTest {
  /** @override */
  get webuiHost() {
    return 'nearby';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kNearbySharing',
      ],
    };
  }
};

[['DeviceIcon', 'nearby_device_icon_test.js'],
 ['Device', 'nearby_device_test.js'],
 ['OnboardingOnePage', 'nearby_onboarding_one_page_test.js'],
 ['OnboardingPage', 'nearby_onboarding_page_test.js'],
 ['PageTemplate', 'nearby_page_template_test.js'],
 ['Preview', 'nearby_preview_test.js'],
 ['Progress', 'nearby_progress_test.js'],
 ['VisibilityPage', 'nearby_visibility_page_test.js'],
 ['ContactVisibility', 'nearby_contact_visibility_test.js'],
].forEach(test => registerTest(...test));

function registerTest(testName, module, caseName) {
  const className = `Nearby${testName}V3Test`;
  this[className] = class extends NearbySharedV3Test {
    /** @override */
    get browsePreload() {
      return `chrome://nearby/test_loader.html?module=chromeos/nearby_share/shared/${
          module}`;
    }
  };

  TEST_F(className, caseName || 'All', () => mocha.run());
}
