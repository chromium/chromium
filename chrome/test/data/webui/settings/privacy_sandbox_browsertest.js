// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for the privacy sandbox UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "content/public/common/content_features.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "third_party/blink/public/common/features.h"');

// eslint-disable-next-line no-var
var PrivacySandboxTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://settings/test_loader.html?module=settings/privacy_sandbox_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'blink::features::kInterestCohortAPIOriginTrial',
        'features::kConversionMeasurement',
        'features::kPrivacySandboxSettings',
      ]
    };
  }
};

TEST_F('PrivacySandboxTest', 'All', function() {
  mocha.run();
});
