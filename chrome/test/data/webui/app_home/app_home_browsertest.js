// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI app home. */
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "content/public/test/browser_test.h"');

GEN('#include "chrome/browser/ui/ui_features.h"');

/* eslint-disable no-var */

var AppHomeTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
};

var AppListAppTest = class extends AppHomeTest {
  /** @override */
  get browsePreload() {
    return 'chrome://apps/test_loader.html?module=app_home/app_list_test.js';
  }

  get featureList() {
    return {enabled: ['features::kDesktopPWAsAppHomePage']};
  }
};

TEST_F('AppListAppTest', 'All', function() {
  mocha.run();
});
