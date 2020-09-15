// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer v2 Nearby Share shared tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/browser_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const NearbySharedBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded('chromeos');
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kNearbySharing']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      '../../test_util.js',
      '../../settings/ensure_lazy_loaded.js',
      'fake_nearby_share_settings.js',
      'fake_nearby_contact_manager.js',
    ]);
  }
};


/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyOnboardingPageTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_onboarding_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_onboarding_page_test.js',
    ]);
  }
};

TEST_F('NearbyOnboardingPageTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyVisibilityPageTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_visibility_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_visibility_page_test.js',
    ]);
  }
};

TEST_F('NearbyVisibilityPageTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyPageTemplateTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_page_template.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_page_template_test.js',
    ]);
  }
};

TEST_F('NearbyPageTemplateTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyContactVisibilityTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_contact_visibility.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_contact_visibility_test.js',
    ]);
  }
};

TEST_F('NearbyContactVisibilityTest', 'All', () => mocha.run());
