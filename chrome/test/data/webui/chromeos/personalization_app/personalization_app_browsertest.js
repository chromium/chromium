// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

var PersonalizationAppBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://personalization/test_loader.html?' +
        'module=chromeos/personalization_app/' +
        'personalization_app_unified_test.js';
  }

  get featureList() {
    return {enabled: ['chromeos::features::kWallpaperWebUI']};
  }
};

TEST_F('PersonalizationAppBrowserTest', 'All', () => mocha.run());
