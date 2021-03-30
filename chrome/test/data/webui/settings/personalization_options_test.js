// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {isChromeOS} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrivacyPageBrowserProxyImpl, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestPrivacyPageBrowserProxy} from 'chrome://test/settings/test_privacy_page_browser_proxy.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.js';
import {eventToPromise, isChildVisible, isVisible} from 'chrome://test/test_util.m.js';

// clang-format on

suite('PersonalizationOptionsTests_AllBuilds', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SyncBrowserProxy} */
  let syncBrowserProxy;

  /** @type {SettingsPersonalizationOptionsElement} */
  let testElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      driveSuggestAvailable: true,
      signinAvailable: true,
    });
  });

  function buildTestElement() {
    PolymerTest.clearBody();
    testElement = document.createElement('settings-personalization-options');
    testElement.prefs = {
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
    };
    document.body.appendChild(testElement);
    flush();
  }

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = syncBrowserProxy;
    buildTestElement();
  });

  teardown(function() {
    testElement.remove();
  });

  test('DriveSearchSuggestControl', function() {
    assertFalse(!!testElement.$$('#driveSuggestControl'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION
    };
    flush();
    assertTrue(!!testElement.$$('#driveSuggestControl'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.REAUTHENTICATE
    };
    flush();
    assertFalse(!!testElement.$$('#driveSuggestControl'));
  });

  if (!isChromeOS) {
    test('signinAllowedToggle', function() {
      const toggle = testElement.$.signinAllowedToggle;
      assertTrue(isVisible(toggle));

      testElement.syncStatus = {signedIn: false};
      // Check initial setup.
      assertTrue(toggle.checked);
      assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
      assertFalse(!!testElement.$.toast.open);

      // When the user is signed out, clicking the toggle should work
      // normally and the restart toast should be opened.
      toggle.click();
      assertFalse(toggle.checked);
      assertFalse(testElement.prefs.signin.allowed_on_next_startup.value);
      assertTrue(testElement.$.toast.open);

      // Clicking it again, turns the toggle back on. The toast remains
      // open.
      toggle.click();
      assertTrue(toggle.checked);
      assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
      assertTrue(testElement.$.toast.open);

      // Reset toast.
      testElement.$.toast.hide();

      // When the user is part way through sync setup, the toggle should be
      // disabled in an on state.
      testElement.syncStatus = {firstSetupInProgress: true};
      assertTrue(toggle.disabled);
      assertTrue(toggle.checked);

      testElement.syncStatus = {signedIn: true};
      // When the user is signed in, clicking the toggle should open the
      // sign-out dialog.
      assertFalse(!!testElement.$$('settings-signout-dialog'));
      toggle.click();
      return eventToPromise('cr-dialog-open', testElement)
          .then(function() {
            flush();
            // The toggle remains on.
            assertTrue(toggle.checked);
            assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
            assertFalse(testElement.$.toast.open);

            const signoutDialog = testElement.$$('settings-signout-dialog');
            assertTrue(!!signoutDialog);
            assertTrue(signoutDialog.$$('#dialog').open);

            // The user clicks cancel.
            const cancel = signoutDialog.$$('#disconnectCancel');
            cancel.click();

            return eventToPromise('close', signoutDialog);
          })
          .then(function() {
            flush();
            assertFalse(!!testElement.$$('settings-signout-dialog'));

            // After the dialog is closed, the toggle remains turned on.
            assertTrue(toggle.checked);
            assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
            assertFalse(testElement.$.toast.open);

            // The user clicks the toggle again.
            toggle.click();
            return eventToPromise('cr-dialog-open', testElement);
          })
          .then(function() {
            flush();
            const signoutDialog = testElement.$$('settings-signout-dialog');
            assertTrue(!!signoutDialog);
            assertTrue(signoutDialog.$$('#dialog').open);

            // The user clicks confirm, which signs them out.
            const disconnectConfirm = signoutDialog.$$('#disconnectConfirm');
            disconnectConfirm.click();

            return eventToPromise('close', signoutDialog);
          })
          .then(function() {
            flush();
            // After the dialog is closed, the toggle is turned off and the
            // toast is shown.
            assertFalse(toggle.checked);
            assertFalse(testElement.prefs.signin.allowed_on_next_startup.value);
            assertTrue(testElement.$.toast.open);
          });
    });

    // Tests that the "Allow sign-in" toggle is hidden when signin is not
    // available.
    test('signinUnavailable', function() {
      loadTimeData.overrideValues({'signinAvailable': false});
      buildTestElement();  // Rebuild the element after modifying loadTimeData.
      assertFalse(isVisible(testElement.$.signinAllowedToggle));
    });
  }
});

suite('PersonalizationOptionsTests_OfficialBuild', function() {
  /** @type {settings.TestPrivacyPageBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsPersonalizationOptionsElement} */
  let testElement;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('Spellcheck toggle', function() {
    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: ['en-US']}}
    };
    flush();
    assertFalse(testElement.$.spellCheckControl.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: []}}
    };
    flush();
    assertTrue(testElement.$.spellCheckControl.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      browser: {enable_spellchecking: {value: false}},
      spellcheck: {
        dictionaries: {value: ['en-US']},
        use_spelling_service: {value: false}
      }
    };
    flush();
    testElement.$.spellCheckControl.click();
    assertTrue(testElement.prefs.spellcheck.use_spelling_service.value);
  });
});
