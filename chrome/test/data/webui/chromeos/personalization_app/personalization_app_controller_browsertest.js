// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://personalization. Tests individual
 * polymer components in isolation.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var PersonalizationAppControllerBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://personalization/test_loader.html?host=webui-test' +
        '&module=chromeos/personalization_app/' +
        'personalization_app_controller_test.js';
  }

  get featureList() {
    return {enabled: ['chromeos::features::kWallpaperWebUI']};
  }
};

TEST_F('PersonalizationAppControllerBrowserTest', 'All', () => mocha.run());
