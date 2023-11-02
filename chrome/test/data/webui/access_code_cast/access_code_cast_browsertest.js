// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for the WebUI access code cast. */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"');
GEN('#include "chrome/browser/profiles/profile.h"');
GEN('#include "chrome/browser/ui/browser.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "components/prefs/pref_service.h"');
GEN('#include "content/public/test/browser_test.h"');

class AccessCodeCastBrowserTest extends PolymerTest {
  get browsePreload() {
    throw new Error('this is abstract and should be overridden by subclasses');
  }

  get testGenPreamble() {
    return () => {
      GEN('browser()->profile()->GetPrefs()->SetBoolean(');
      GEN('   media_router::prefs::kAccessCodeCastEnabled, true);');
    };
  }

  get featureList() {
    return {enabled: ['features::kAccessCodeCastUI']};
  }
}

var AccessCodeCastAppTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/access_code_cast_app_test.js';
  }
};

TEST_F('AccessCodeCastAppTest', 'All', function() {
  mocha.run();
});

var AccessCodeCastBrowserProxyTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/browser_proxy_test.js';
  }
};

TEST_F('AccessCodeCastBrowserProxyTest', 'All', function() {
  mocha.run();
});

var AccessCodeCastErrorMessageTest = class extends AccessCodeCastBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/error_message_test.js';
  }
};

TEST_F('AccessCodeCastErrorMessageTest', 'All', function() {
  mocha.run();
});

// PasscodeInputTest has started acting flaky (crbug/1363398). Disabling for now pending investigation.

// var AccessCodeCastPasscodeInputTest = class extends AccessCodeCastBrowserTest {
//   /** @override */
//   get browsePreload() {
//     return 'chrome://access-code-cast/test_loader.html?module=access_code_cast/passcode_input_test.js';
//   }
// };

// TEST_F('AccessCodeCastPasscodeInputTest', 'All', function() {
//   mocha.run();
// });
