// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Sign-in web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "base/command_line.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "chrome/common/chrome_features.h"');

class SigninBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  }
}

/**
 * Test fixture for
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var SigninSyncConfirmationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/test_loader.html?module=signin/sync_confirmation_test.js';
  }
};

TEST_F('SigninSyncConfirmationTest', 'Dialog', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/signin_reauth/signin_reauth.html.
 */
// eslint-disable-next-line no-var
var SigninReauthTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    // See signin_metrics::ReauthAccessPoint for definition of the
    // "access_point" parameter.
    return 'chrome://signin-reauth/test_loader.html?module=signin/signin_reauth_test.js&access_point=2';
  }
};

TEST_F('SigninReauthTest', 'Dialog', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/dice_web_signin_intercept/dice_web_signin_intercept.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var DiceWebSigninInterceptTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://signin-dice-web-intercept/test_loader.html?module=signin/dice_web_signin_intercept_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kEnableEphemeralGuestProfilesOnDesktop',
      ]
    };
  }
};

TEST_F('DiceWebSigninInterceptTest', 'Bubble', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_creation_flow/profile_type_choice.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfileTypeChoiceTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_type_choice_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSignInProfileCreation',
        'features::kNewProfilePicker',
      ]
    };
  }
};

TEST_F('ProfileTypeChoiceTest', 'Buttons', function() {
  mocha.run();
});


/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_creation_flow/local_profile_customization.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var LocalProfileCustomizationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/local_profile_customization_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kNewProfilePicker',
      ]
    };
  }
};

TEST_F('LocalProfileCustomizationTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_picker_app.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfilePickerAppTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_picker_app_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSignInProfileCreation',
        'features::kNewProfilePicker',
      ]
    };
  }
};

TEST_F('ProfilePickerAppTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_picker_main_view.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfilePickerMainViewTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_picker_main_view_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kSignInProfileCreation',
        'features::kNewProfilePicker',
      ]
    };
  }
};

TEST_F('ProfilePickerMainViewTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_card_menu.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfileCardMenuTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_card_menu_test.js';
  }

  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kNewProfilePicker',
      ]
    };
  }
};

TEST_F('ProfileCardMenuTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_switch.js.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfileSwitchTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_switch_test.js';
  }
};

TEST_F('ProfileSwitchTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_customization/profile_customization_app.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
// eslint-disable-next-line no-var
var ProfileCustomizationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-customization/test_loader.html?module=signin/profile_customization_test.js';
  }
};

TEST_F('ProfileCustomizationTest', 'Bubble', function() {
  mocha.run();
});
