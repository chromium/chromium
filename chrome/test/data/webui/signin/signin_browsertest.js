// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Sign-in web UI tests. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "base/command_line.h"');
GEN('#include "build/branding_buildflags.h"');
GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "chrome/browser/signin/signin_features.h"');
GEN('#include "chrome/browser/ui/ui_features.h"');
GEN('#include "components/signin/public/base/signin_buildflags.h"');
GEN('#include "components/signin/public/base/signin_switches.h"')
GEN('#include "content/public/test/browser_test.h"');

// Keep enum values in sync with the SyncConfirmationStyle enum class defined in
// signin_url_utils.h.
const SyncConfirmationStyle = {
  DEFAULT_MODAL: 0,
  SIGNIN_INTERCEPT_MODAL: 1,
  WINDOW: 2,
};

class SigninBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    throw new Error('this is abstract and should be overriden by subclasses');
  }
}

/**
 * Test fixture for the default modal dialog version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var LegacySyncDefaultModalTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/test_loader.html?module=' +
        'signin/sync_confirmation_test.js';
  }

  /** @override */
  get featureList() {
    return {disabled: ['switches::kTangibleSync']};
  }
};

TEST_F('LegacySyncDefaultModalTest', 'Dialog', function() {
  mocha.run();
});

/**
 * Test fixture for the Signin Intercept modal dialog version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var LegacySyncSigninInterceptModalTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return `chrome://sync-confirmation/test_loader.html?module=` +
        `signin/sync_confirmation_test.js&style=` +
        `${SyncConfirmationStyle.SIGNIN_INTERCEPT_MODAL}`;
  }

  /** @override */
  get featureList() {
    return {
      enabled: ['kSyncPromoAfterSigninIntercept'],
      disabled: ['switches::kTangibleSync']
    };
  }
};

TEST_F('LegacySyncSigninInterceptModalTest', 'Dialog', function() {
  mocha.run();
});

/**
 * Test fixture for the window version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var LegacySyncWindowTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return `chrome://sync-confirmation/test_loader.html?module=` +
        `signin/sync_confirmation_test.js&style=` +
        `${SyncConfirmationStyle.WINDOW}`;
  }

  /** @override */
  get featureList() {
    return {disabled: ['switches::kTangibleSync']};
  }
};

TEST_F('LegacySyncWindowTest', 'Window', function() {
  mocha.run();
});

/**
 * Test fixture for the default modal dialog version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html
 * with Tangible Sync enabled.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var TangibleSyncDefaultModalTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://sync-confirmation/test_loader.html?module=' +
        'signin/sync_confirmation_test.js';
  }

  /** @override */
  get featureList() {
    return {enabled: ['switches::kTangibleSync']};
  }
};

TEST_F('TangibleSyncDefaultModalTest', 'Dialog', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(ENABLE_DICE_SUPPORT)');
/**
 * Test fixture for the Signin Intercept modal dialog version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html
 * with Tangible Sync enabled.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var TangibleSyncSigninInterceptModalTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return `chrome://sync-confirmation/test_loader.html?module=` +
        `signin/sync_confirmation_test.js&style=` +
        `${SyncConfirmationStyle.SIGNIN_INTERCEPT_MODAL}`;
  }

  /** @override */
  get featureList() {
    return {
      enabled: ['kSyncPromoAfterSigninIntercept', 'switches::kTangibleSync']
    };
  }
};

TEST_F('TangibleSyncSigninInterceptModalTest', 'Dialog', function() {
  mocha.run();
});
GEN('#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)');

/**
 * Test fixture for the window version of
 * chrome/browser/resources/signin/sync_confirmation/sync_confirmation.html
 * with Tangible Sync enabled.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var TangibleSyncWindowTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return `chrome://sync-confirmation/test_loader.html?module=` +
        `signin/sync_confirmation_test.js&style=` +
        `${SyncConfirmationStyle.WINDOW}`;
  }

  /** @override */
  get featureList() {
    return {enabled: ['switches::kTangibleSync']};
  }
};

TEST_F('TangibleSyncWindowTest', 'Window', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(ENABLE_DICE_SUPPORT)');
/**
 * Test fixture for
 * chrome/browser/resources/signin/signin_reauth/signin_reauth.html.
 */
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
var DiceWebSigninInterceptTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://signin-dice-web-intercept/test_loader.html?module=signin/dice_web_signin_intercept_test.js';
  }
};

TEST_F('DiceWebSigninInterceptTest', 'Bubble', function() {
  mocha.run();
});
GEN('#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)');

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_creation_flow/profile_type_choice.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var ProfileTypeChoiceTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_type_choice_test.js';
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
var LocalProfileCustomizationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/local_profile_customization_test.js';
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
var ProfilePickerAppTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_picker_app_test.js';
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
var ProfilePickerMainViewTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_picker_main_view_test.js';
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
var ProfileCardMenuTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/profile_card_menu_test.js';
  }
};

TEST_F('ProfileCardMenuTest', 'All', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_switch.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
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
var ProfileCustomizationTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-customization/test_loader.html?module=signin/profile_customization_test.js';
  }
};

TEST_F('ProfileCustomizationTest', 'Bubble', function() {
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/signin/enterprise_profile_welcome/enterprise_profile_welcome.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var SigninEnterpriseProfileWelcomeTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://enterprise-profile-welcome/test_loader.html?module=signin/enterprise_profile_welcome_test.js';
  }
};

TEST_F('SigninEnterpriseProfileWelcomeTest', 'Dialog', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS_LACROS)');
/**
 * Test fixture for
 * chrome/browser/resources/signin/profile_picker/profile_creation_flow/account_selection_lacros.html.
 * This has to be declared as a variable for TEST_F to find it correctly.
 */
var AccountSelectionLacrosTest = class extends SigninBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://profile-picker/test_loader.html?module=signin/account_selection_lacros_test.js';
  }
};

TEST_F('AccountSelectionLacrosTest', 'All', function() {
  mocha.run();
});
GEN('#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)');
