// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI app settings. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"');

class AppSettingsBrowserTest extends PolymerTest {
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }

  testGenPreamble() {
    GEN('WebAppSettingsNavigationThrottle::DisableForTesting();');
  }
}

var AppSettingsAppTest = class extends AppSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://app-settings/test_loader.html?module=app_settings/app_test.js';
  }
};

TEST_F('AppSettingsAppTest', 'All', function() {
  mocha.run();
});
