// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Borealis Installer page.
 */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "ash/constants/ash_features.h"');

var BorealisInstallerAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://borealis-installer/test_loader.html?module=' +
        'chromeos/borealis_installer/borealis_installer_app_test.js';
  }

  get featureList() {
    return {enabled: ['ash::features::kBorealisWebUIInstaller']};
  }
};

TEST_F('BorealisInstallerAppBrowserTest', 'All', () => mocha.run());
