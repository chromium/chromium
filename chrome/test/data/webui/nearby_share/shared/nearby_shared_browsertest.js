// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer v2 Nearby Share shared tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"');
GEN('#include "content/public/test/browser_test.h"');

const NearbySharedBrowserTest = class extends Polymer2DeprecatedTest {
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
var NearbyDeviceIconTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_device_icon.html';
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_device_icon_test.js',
    ]);
  }
};

TEST_F('NearbyDeviceIconTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyDeviceTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_device.html';
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_device_test.js',
    ]);
  }
};

TEST_F('NearbyDeviceTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyOnboardingPageTest = class extends NearbySharedBrowserTest {
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
var NearbyPreviewTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_preview.html';
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_preview_test.js',
    ]);
  }
};

TEST_F('NearbyPreviewTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyProgressTest = class extends NearbySharedBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'shared/nearby_progress.html';
  }
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_progress_test.js',
    ]);
  }
};

TEST_F('NearbyProgressTest', 'All', () => mocha.run());

/**
 * @extends {NearbySharedBrowserTest}
 */
var NearbyContactVisibilityTest = class extends NearbySharedBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'nearby_contact_visibility_test.js',
    ]);
  }
};

TEST_F('NearbyContactVisibilityTest', 'All', () => mocha.run());
