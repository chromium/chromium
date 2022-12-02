// Copyright 2020 The Chromium Authors
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
TEST_F('InlineLoginBrowserTest', 'OkButton', function() {
  this.runMochaTest(inline_login_test.TestNames.OkButton);
});
GEN('#endif')

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');
// TODO(crbug.com/1347746): Merge this test suite with the test above after the
// feature is launched.
var InlineLoginBrowserTestWithArcAccountRestrictionsEnabled =
    class extends InlineLoginBrowserTest {
  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kLacrosSupport',
      ],
    };
  }
};

TEST_F(
    'InlineLoginBrowserTestWithArcAccountRestrictionsEnabled', 'Initialize',
    function() {
      this.runMochaTest(inline_login_test.TestNames.Initialize);
    });

TEST_F(
    'InlineLoginBrowserTestWithArcAccountRestrictionsEnabled', 'WebUICallbacks',
    function() {
      this.runMochaTest(inline_login_test.TestNames.WebUICallbacks);
    });

TEST_F(
    'InlineLoginBrowserTestWithArcAccountRestrictionsEnabled',
    'AuthExtHostCallbacks', function() {
      this.runMochaTest(inline_login_test.TestNames.AuthExtHostCallbacks);
    });

TEST_F(
    'InlineLoginBrowserTestWithArcAccountRestrictionsEnabled', 'BackButton',
    function() {
      this.runMochaTest(inline_login_test.TestNames.BackButton);
    });

var InlineLoginWelcomePageBrowserTest = class extends InlineLoginBrowserTest {
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

// TODO(crbug.com/1347746): Make this test the default one, and remove the test
// suite above when the feature is enabled by default.
var InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled =
    class extends InlineLoginWelcomePageBrowserTest {
  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kLacrosSupport',
      ],
    };
  }
};

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'Reauthentication', function() {
      this.runMochaTest(
          inline_login_welcome_page_test.TestNames.Reauthentication);
    });

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'OkButton', function() {
      this.runMochaTest(inline_login_welcome_page_test.TestNames.OkButton);
    });

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'GoBack', function() {
      this.runMochaTest(inline_login_welcome_page_test.TestNames.GoBack);
    });

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'IsAvailableInArc', function() {
      this.runMochaTest(
          inline_login_welcome_page_test.TestNames.IsAvailableInArc);
    });

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'ToggleHidden', function() {
      this.runMochaTest(inline_login_welcome_page_test.TestNames.ToggleHidden);
    });

TEST_F(
    'InlineLoginWelcomePageBrowserTestWithArcAccountRestrictionsEnabled',
    'LinkClick', function() {
      this.runMochaTest(inline_login_welcome_page_test.TestNames.LinkClick);
    });

var InlineLoginArcAccountPickerBrowserTest =
    class extends InlineLoginBrowserTest {
  /** @override */
  get browsePreload() {
    // See Reason enum in components/signin/public/base/signin_metrics.h.
    return 'chrome://chrome-signin/test_loader.html?module=inline_login/arc_account_picker_page_test.js&reason=5';
  }

  get suiteName() {
    return arc_account_picker_page_test.suiteName;
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'ash::features::kLacrosSupport',
      ],
    };
  }
};

TEST_F('InlineLoginArcAccountPickerBrowserTest', 'ArcPickerActive', function() {
  this.runMochaTest(arc_account_picker_page_test.TestNames.ArcPickerActive);
});

TEST_F(
    'InlineLoginArcAccountPickerBrowserTest', 'ArcPickerHiddenForReauth',
    function() {
      this.runMochaTest(
          arc_account_picker_page_test.TestNames.ArcPickerHiddenForReauth);
    });

TEST_F(
    'InlineLoginArcAccountPickerBrowserTest', 'ArcPickerHiddenNoAccounts',
    function() {
      this.runMochaTest(
          arc_account_picker_page_test.TestNames.ArcPickerHiddenNoAccounts);
    });

TEST_F('InlineLoginArcAccountPickerBrowserTest', 'AddAccount', function() {
  this.runMochaTest(arc_account_picker_page_test.TestNames.AddAccount);
});

TEST_F(
    'InlineLoginArcAccountPickerBrowserTest', 'MakeAvailableInArc', function() {
      this.runMochaTest(
          arc_account_picker_page_test.TestNames.MakeAvailableInArc);
    });

var InlineLoginSigninBlockedByPolicyPageBrowserTest =
    class extends InlineLoginBrowserTest {
  get browsePreload() {
    // Reason 1: Add secondary account.
    // See Reason enum in components/signin/public/base/signin_metrics.h.
    return 'chrome://chrome-signin/test_loader.html?module=inline_login/inline_login_signin_blocked_by_policy_page_test.js&reason=1';
  }

  get suiteName() {
    return inline_login_signin_blocked_by_policy_page_test.suiteName;
  }
};

TEST_F(
    'InlineLoginSigninBlockedByPolicyPageBrowserTest', 'BlockedSigninPage',
    function() {
      this.runMochaTest(inline_login_signin_blocked_by_policy_page_test
                            .TestNames.BlockedSigninPage);
    });

TEST_F(
    'InlineLoginSigninBlockedByPolicyPageBrowserTest', 'OkButton', function() {
      this.runMochaTest(
          inline_login_signin_blocked_by_policy_page_test.TestNames.OkButton);
    });

TEST_F(
    'InlineLoginSigninBlockedByPolicyPageBrowserTest',
    'FireWebUIListenerCallback', function() {
      this.runMochaTest(inline_login_signin_blocked_by_policy_page_test
                            .TestNames.FireWebUIListenerCallback);
    });
GEN('#endif');
