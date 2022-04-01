// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer access code cast tests on access code cast UI. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/build_config.h"');
GEN('#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "components/prefs/pref_service.h"');
GEN('#include "content/public/test/browser_test.h"');

/** Test fixture for Polymer AccessCodeCast elements. */
const AccessCodeCastBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  /** @override */
  get webuiHost() {
    return 'access-code-cast-app';
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kAccessCodeCastUI']};
  }

  /** @override */
  get testGenPreamble() {
    return () => {
      GEN('browser()->profile()->GetPrefs()->SetBoolean(');
      GEN('   media_router::prefs::kAccessCodeCastEnabled, true);');
    };
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
GEN('#if BUILDFLAG(IS_MAC)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');
TEST_F('AccessCodeCastAppTest', 'MAYBE_All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var AccessCodeCastBrowserProxyTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/browser_proxy_test.js';
  }
};

TEST_F('AccessCodeCastBrowserProxyTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var AccessCodeCastErrorMessageElementTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/error_message_test.js';
  }
};

TEST_F('AccessCodeCastErrorMessageElementTest', 'All', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var AccessCodeCastPasscodeInputElementTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/passcode_input_test.js';
  }
};

TEST_F('AccessCodeCastPasscodeInputElementTest', 'All', function() {
  mocha.run();
});
