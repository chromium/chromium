// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the CFM Network Settings JS.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

var CfmNetworkSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://cfm-network-settings/test_loader.html?module=chromeos/chromebox_for_meetings/cfm_network_settings_test.js';
  }
};

TEST_F('CfmNetworkSettingsBrowserTest', 'All', function() {
  mocha.run();
});
