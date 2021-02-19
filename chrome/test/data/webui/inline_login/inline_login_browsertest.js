// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for the Inline login flow ('Add account' flow) on
 * ChromeOS and desktop Chrome.
 */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
GEN('#include "ash/constants/ash_features.h"');
GEN('#endif');

// eslint-disable-next-line no-var
var InlineLoginBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    // See Reason enum in components/signin/public/base/signin_metrics.h.
    return 'chrome://chrome-signin/test_loader.html?module=inline_login/inline_login_test.js&reason=5';
  }

  get suiteName() {
    return inline_login_test.suiteName;
  }

  /** @param {string} testName The name of the test to run. */
  runMochaTest(testName) {
    runMochaTest(this.suiteName, testName);
  }
};

TEST_F('InlineLoginBrowserTest', 'Initialize', function() {
  this.runMochaTest(inline_login_test.TestNames.Initialize);
});

TEST_F('InlineLoginBrowserTest', 'WebUICallbacks', function() {
  this.runMochaTest(inline_login_test.TestNames.WebUICallbacks);
});

TEST_F('InlineLoginBrowserTest', 'AuthExtHostCallbacks', function() {
  this.runMochaTest(inline_login_test.TestNames.AuthExtHostCallbacks);
});

TEST_F('InlineLoginBrowserTest', 'BackButton', function() {
  this.runMochaTest(inline_login_test.TestNames.BackButton);
});

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
// eslint-disable-next-line no-var
var InlineLoginBrowserTestWithAccountManagementFlowsV2Enabled =
    class extends InlineLoginBrowserTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }
};

TEST_F(
    'InlineLoginBrowserTestWithAccountManagementFlowsV2Enabled', 'Initialize',
    function() {
      this.runMochaTest(inline_login_test.TestNames.Initialize);
    });

TEST_F(
    'InlineLoginBrowserTestWithAccountManagementFlowsV2Enabled',
    'WebUICallbacks', function() {
      this.runMochaTest(inline_login_test.TestNames.WebUICallbacks);
    });

TEST_F(
    'InlineLoginBrowserTestWithAccountManagementFlowsV2Enabled',
    'AuthExtHostCallbacks', function() {
      this.runMochaTest(inline_login_test.TestNames.AuthExtHostCallbacks);
    });

TEST_F(
    'InlineLoginBrowserTestWithAccountManagementFlowsV2Enabled', 'BackButton',
    function() {
      this.runMochaTest(inline_login_test.TestNames.BackButton);
    });

// eslint-disable-next-line no-var
var InlineLoginWelcomePageBrowserTest = class extends InlineLoginBrowserTest {
  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kAccountManagementFlowsV2']};
  }

  /** @override */
  get browsePreload() {
    // See Reason enum in components/signin/public/base/signin_metrics.h.
    return 'chrome://chrome-signin/test_loader.html?module=inline_login/inline_login_welcome_page_test.js&reason=5';
  }

  get suiteName() {
    return inline_login_welcome_page_test.suiteName;
  }
};

TEST_F('InlineLoginWelcomePageBrowserTest', 'Reauthentication', function() {
  this.runMochaTest(inline_login_welcome_page_test.TestNames.Reauthentication);
});

TEST_F('InlineLoginWelcomePageBrowserTest', 'OkButton', function() {
  this.runMochaTest(inline_login_welcome_page_test.TestNames.OkButton);
});

TEST_F('InlineLoginWelcomePageBrowserTest', 'Checkbox', function() {
  this.runMochaTest(inline_login_welcome_page_test.TestNames.Checkbox);
});

TEST_F('InlineLoginWelcomePageBrowserTest', 'GoBack', function() {
  this.runMochaTest(inline_login_welcome_page_test.TestNames.GoBack);
});
GEN('#endif');
