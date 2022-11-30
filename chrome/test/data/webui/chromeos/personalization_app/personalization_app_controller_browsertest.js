// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://personalization. Tests individual
 * polymer components in isolation.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "ash/webui/personalization_app/test/personalization_app_mojom_banned_browsertest_fixture.h"');
GEN('#include "content/public/test/browser_test.h"');

var PersonalizationAppControllerBrowserTest = class extends PolymerTest {
  get browsePreload() {
    return 'chrome://personalization/test_loader.html' +
        '?module=chromeos/personalization_app/' +
        'personalization_app_controller_test.js';
  }

  get typedefCppFixture() {
    return 'ash::personalization_app::' +
        'PersonalizationAppMojomBannedBrowserTestFixture';
  }
};

TEST_F('PersonalizationAppControllerBrowserTest', 'All', () => mocha.run());
