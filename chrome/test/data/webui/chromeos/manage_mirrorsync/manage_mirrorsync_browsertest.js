// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://manage-mirrorsync. Attempts to test the
 * entire application as a blackbox instead of individual components. See
 * manage_mirrorsync_app_test.ts for individual tests.
 */

GEN_INCLUDE(['//chrome/test/data/webui/chromeos/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var ManageMirrorSyncAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://manage-mirrorsync/test_loader.html?module=' +
        'chromeos/manage_mirrorsync/manage_mirrorsync_app_test.js';
  }

  get featureList() {
    return {enabled: ['ash::features::kDriveFsMirroring']};
  }
};

TEST_F('ManageMirrorSyncAppBrowserTest', 'All', () => mocha.run());
