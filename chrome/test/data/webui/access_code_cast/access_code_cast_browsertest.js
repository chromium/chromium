// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer access code cast tests on access code cast UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer AccessCodeCast elements. */
const AccessCodeCastBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overridden by subclasses';
  }

  /** @override */
  get webuiHost() {
    return 'access-code-cast-app';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kAccessCodeCastUI']};
  }
};

// eslint-disable-next-line no-var
var AccessCodeCastAppTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/access_code_cast_test.js';
  }
};

/**
 * This browsertest acts as a thin wrapper to run the unit tests found
 * at access_code_cast_test.js
 */
TEST_F('AccessCodeCastAppTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var AccessCodeCastCodeInputElementTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/code_input_test.js';
  }
};

/**
 * This browsertest acts as a thin wrapper to run the unit tests found
 * at code_input_test.js
 */
TEST_F('AccessCodeCastCodeInputElementTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var AccessCodeCastErrorMessageElementTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/error_message_test.js';
  }
};

/**
 * This browsertest acts as a thin wrapper to run the unit tests found
 * at code_input_test.js
 */
TEST_F('AccessCodeCastErrorMessageElementTest', 'All', function() {
  mocha.run();
});
