// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on welcome UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/ui/webui/welcome/helpers.h"');
GEN('#include "services/network/public/cpp/features.h"');

/** Test fixture for Polymer welcome elements. */
const WelcomeBrowserTest = class extends PolymerTest {
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
    return 'welcome';
  }

  /** @override */
  get featureList() {
    return {
      enabled: ['welcome::kForceEnabled', 'network::features::kOutOfBlinkCors']
    };
  }
};

// eslint-disable-next-line no-var
var WelcomeAppChooserTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/app_chooser_test.js';
  }
};

TEST_F('WelcomeAppChooserTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeWelcomeAppTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/welcome_app_test.js';
  }
};

TEST_F('WelcomeWelcomeAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeSigninViewTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/signin_view_test.js';
  }
};

TEST_F('WelcomeSigninViewTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeNavigationBehaviorTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/navigation_behavior_test.js';
  }
};

TEST_F('WelcomeNavigationBehaviorTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeModuleMetricsTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/module_metrics_test.js';
  }
};

TEST_F('WelcomeModuleMetricsTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeSetAsDefaultTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/nux_set_as_default_test.js';
  }
};

TEST_F('WelcomeSetAsDefaultTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var WelcomeNtpBackgroundTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/nux_ntp_background_test.js';
  }
};

TEST_F('WelcomeNtpBackgroundTest', 'All', function() {
  mocha.run();
});
