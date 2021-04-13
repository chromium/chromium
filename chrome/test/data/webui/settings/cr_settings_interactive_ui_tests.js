// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for shared Polymer 3 elements. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for shared Polymer 3 elements. */
// eslint-disable-next-line no-var
var CrSettingsV3InteractiveUITest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings';
  }
};

// eslint-disable-next-line no-var
var CrSettingsAnimatedPagesV3Test =
    class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_animated_pages_test.js';
  }
};

TEST_F('CrSettingsAnimatedPagesV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsPaymentsSectionV3Test =
    class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/payments_section_interactive_test.js';
  }
};

TEST_F('CrSettingsPaymentsSectionV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSyncPageV3Test = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/people_page_sync_page_interactive_test.js';
  }
};

TEST_F('CrSettingsSyncPageV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var CrSettingsSecureDnsV3Test = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/secure_dns_interactive_test.js';
  }
};

TEST_F('CrSettingsSecureDnsV3Test', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var SettingsUIV3InteractiveTest = class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_ui_tests.js';
  }
};

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_SettingsUIToolbarAndDrawer DISABLED_SettingsUIToolbarAndDrawer');
GEN('#else');
GEN('#define MAYBE_SettingsUIToolbarAndDrawer SettingsUIToolbarAndDrawer');
GEN('#endif');
TEST_F(
    'SettingsUIV3InteractiveTest', 'MAYBE_SettingsUIToolbarAndDrawer',
    function() {
      runMochaSuite('SettingsUIToolbarAndDrawer');
    });

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_SettingsUIAdvanced DISABLED_SettingsUIAdvanced');
GEN('#else');
GEN('#define MAYBE_SettingsUIAdvanced SettingsUIAdvanced');
GEN('#endif');
TEST_F('SettingsUIV3InteractiveTest', 'MAYBE_SettingsUIAdvanced', function() {
  runMochaSuite('SettingsUIAdvanced');
});

// Times out on Mac. See https://crbug.com/1060981.
GEN('#if defined(OS_MAC)');
GEN('#define MAYBE_SettingsUISearch DISABLED_SettingsUISearch');
GEN('#else');
GEN('#define MAYBE_SettingsUISearch SettingsUISearch');
GEN('#endif');
TEST_F('SettingsUIV3InteractiveTest', 'MAYBE_SettingsUISearch', function() {
  runMochaSuite('SettingsUISearch');
});

// eslint-disable-next-line no-var
var CrSettingsMenuV3InteractiveTest =
    class extends CrSettingsV3InteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/settings_menu_interactive_ui_test.js';
  }
};

TEST_F('CrSettingsMenuV3InteractiveTest', 'All', function() {
  mocha.run();
});
