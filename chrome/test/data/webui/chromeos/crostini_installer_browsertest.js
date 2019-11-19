// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Crostini Installer page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chromeos/constants/chromeos_features.h"');
GEN('#include "services/network/public/cpp/features.h"');

function CrostiniInstallerBrowserTest() {}

CrostiniInstallerBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload:
      'chrome://crostini-installer/test_loader.html?module=chromeos/crostini_installer_app_test.js',

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
  ],

  featureList: {
    enabled: [
      'chromeos::features::kCrostiniWebUIInstaller',
      'network::features::kOutOfBlinkCors'
    ]
  },
};


TEST_F('CrostiniInstallerBrowserTest', 'All', function() {
  mocha.run();
});
