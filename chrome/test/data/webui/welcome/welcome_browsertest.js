// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer welcome tests on welcome UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/webui/welcome/helpers.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer welcome elements. */
const WelcomeBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'welcome';
  }

  /** @override */
  get featureList() {
    return {enabled: ['welcome::kForceEnabled']};
  }
};

var WelcomeAppChooserTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/app_chooser_test.js';
  }
};

TEST_F('WelcomeAppChooserTest', 'All', function() {
  mocha.run();
});

var WelcomeWelcomeAppTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/welcome_app_test.js';
  }
};

TEST_F('WelcomeWelcomeAppTest', 'All', function() {
  mocha.run();
});

var WelcomeSigninViewTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/signin_view_test.js';
  }
};

TEST_F('WelcomeSigninViewTest', 'All', function() {
  mocha.run();
});

var WelcomeNavigationBehaviorTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/navigation_mixin_test.js';
  }
};

TEST_F('WelcomeNavigationBehaviorTest', 'All', function() {
  mocha.run();
});

var WelcomeModuleMetricsTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/module_metrics_test.js';
  }
};

TEST_F('WelcomeModuleMetricsTest', 'All', function() {
  mocha.run();
});

var WelcomeSetAsDefaultTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/nux_set_as_default_test.js';
  }
};

TEST_F('WelcomeSetAsDefaultTest', 'All', function() {
  mocha.run();
});

var WelcomeNtpBackgroundTest = class extends WelcomeBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://welcome/test_loader.html?module=welcome/nux_ntp_background_test.js';
  }
};

TEST_F('WelcomeNtpBackgroundTest', 'All', function() {
  mocha.run();
});
