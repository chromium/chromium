// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AutofillAssistantBrowserProxyImpl, SettingsPersonalizationOptionsElement} from 'chrome://settings/lazy_load.js';
import {PrivacyPageVisibility} from 'chrome://settings/page_visibility.js';
import {loadTimeData, PrivacyPageBrowserProxyImpl, SettingsToggleButtonElement, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// <if expr="not is_chromeos">
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// </if>

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {TestAutofillAssistantBrowserProxy} from './test_autofill_assistant_browser_proxy.js';

// clang-format on

suite('PersonalizationOptionsTests_AllBuilds', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let autofillAssistantBrowserProxy: TestAutofillAssistantBrowserProxy;
  let customPageVisibility: PrivacyPageVisibility;
  let testElement: SettingsPersonalizationOptionsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      driveSuggestAvailable: true,
      isAutomatedPasswordChangeEnabled: true,
      signinAvailable: true,
      changePriceEmailNotificationsEnabled: true,
    });
  });

  function buildTestElement() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    testElement = document.createElement('settings-personalization-options');
    testElement.prefs = {
      autofill_assistant: {enabled: {value: false}},
      signin: {
        allowed_on_next_startup:
            {type: chrome.settingsPrivate.PrefType.BOOLEAN, value: true},
      },
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      price_tracking: {email_notifications_enabled: {value: false}},
    };
    testElement.pageVisibility = customPageVisibility;
    document.body.appendChild(testElement);
    flush();
  }

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    autofillAssistantBrowserProxy = new TestAutofillAssistantBrowserProxy();
    AutofillAssistantBrowserProxyImpl.setInstance(
        autofillAssistantBrowserProxy);
    buildTestElement();
  });

  teardown(function() {
    testElement.remove();
  });

  test('DriveSearchSuggestControl', function() {
    assertFalse(
        !!testElement.shadowRoot!.querySelector('#driveSuggestControl'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(!!testElement.shadowRoot!.querySelector('#driveSuggestControl'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    flush();
    assertFalse(
        !!testElement.shadowRoot!.querySelector('#driveSuggestControl'));
  });

  // <if expr="not is_chromeos">
  test('signinAllowedToggle', function() {
    const toggle = testElement.$.signinAllowedToggle;
    assertTrue(isVisible(toggle));

    testElement.syncStatus = {
      signedIn: false,
      statusAction: StatusAction.NO_ACTION,
    };
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
    testElement.syncStatus = {
      firstSetupInProgress: true,
      statusAction: StatusAction.NO_ACTION,
    };
    assertTrue(toggle.disabled);
    assertTrue(toggle.checked);

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    // When the user is signed in, clicking the toggle should open the
    // sign-out dialog.
    assertFalse(
        !!testElement.shadowRoot!.querySelector('settings-signout-dialog'));
    toggle.click();
    return eventToPromise('cr-dialog-open', testElement)
        .then(function() {
          flush();
          // The toggle remains on.
          assertTrue(toggle.checked);
          assertTrue(testElement.prefs.signin.allowed_on_next_startup.value);
          assertFalse(testElement.$.toast.open);

          const signoutDialog =
              testElement.shadowRoot!.querySelector('settings-signout-dialog');
          assertTrue(!!signoutDialog);
          assertTrue(signoutDialog!.$.dialog.open);

          // The user clicks cancel.
          const cancel = signoutDialog!.shadowRoot!.querySelector<HTMLElement>(
              '#disconnectCancel')!;
          cancel.click();

          return eventToPromise('close', signoutDialog!);
        })
        .then(function() {
          flush();
          assertFalse(!!testElement.shadowRoot!.querySelector(
              'settings-signout-dialog'));

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
          const signoutDialog =
              testElement.shadowRoot!.querySelector('settings-signout-dialog');
          assertTrue(!!signoutDialog);
          assertTrue(signoutDialog!.$.dialog.open);

          // The user clicks confirm, which signs them out.
          const disconnectConfirm =
              signoutDialog!.shadowRoot!.querySelector<HTMLElement>(
                  '#disconnectConfirm')!;
          disconnectConfirm.click();

          return eventToPromise('close', signoutDialog!);
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

  test('searchSuggestToggleShownIfPageVisibilityUndefined', function() {
    // This is the most common case, as in non-Guest profiles on Desktop
    // platforms pageVisibility is undefined.
    assertTrue(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });

  test('searchSuggestToggleHiddenByPageVisibility', function() {
    customPageVisibility = {
      searchPrediction: false,
      networkPrediction: false,
    };
    buildTestElement();
    assertFalse(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });

  test('searchSuggestToggleShownByPageVisibility', function() {
    customPageVisibility = {
      searchPrediction: true,
      networkPrediction: false,
    };
    buildTestElement();
    assertTrue(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });
  // </if>

  test('autofillAssistantAvailable', function() {
    // If the user is not logged in, the element is hidden.
    testElement.syncStatus = {
      signedIn: false,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertFalse(isVisible(testElement.shadowRoot!.querySelector(
        '#enableAutofillAssistantToggle')));

    // For logged in users, the toggle appears.
    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(isVisible(testElement.shadowRoot!.querySelector(
        '#enableAutofillAssistantToggle')));
  });

  test('autofillAssistant toggle', async function() {
    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();

    // Initially, the toggle is off.
    const toggle =
        testElement.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#enableAutofillAssistantToggle');
    assertTrue(!!toggle);
    assertFalse(toggle.checked);

    // Clicking it leads to a consent prompt.
    toggle.click();
    await (autofillAssistantBrowserProxy.whenCalled('promptForConsent'));
    // The TestAutofillAssistantBrowserProxy simulates accepting the prompt.
    assertTrue(toggle.checked);
    assertTrue(testElement.prefs.autofill_assistant.enabled.value);

    // Clicking it again turns it off and logs that consent was revoked.
    toggle.click();
    await (autofillAssistantBrowserProxy.whenCalled('revokeConsent'));
    assertFalse(toggle.checked);
    assertFalse(testElement.prefs.autofill_assistant.enabled.value);
  });

  test('autofillAssistantUnavailable', function() {
    loadTimeData.overrideValues({'isAutomatedPasswordChangeEnabled': false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.
    assertFalse(isVisible(testElement.shadowRoot!.querySelector(
        '#enableAutofillAssistantToggle')));
  });

  test('priceEmailNotificationsToggleShown', function() {
    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test('priceEmailNotificationsToggleHidden', function() {
    loadTimeData.overrideValues(
        {'changePriceEmailNotificationsEnabled': false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));

    testElement.syncStatus = {
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });
});

suite('PersonalizationOptionsTests_OfficialBuild', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsPersonalizationOptionsElement;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  test('Spellcheck toggle', function() {
    // <if expr="chromeos_ash">
    // On ChromeOS spellcheck toggle is shown in OS settings only.
    loadTimeData.overrideValues({
      isOSSettings: true,
    });
    // </if>

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: ['en-US']}},
    };
    flush();
    const shadowRoot = testElement.shadowRoot!;
    assertFalse(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);
    // <if expr="chromeos_ash">
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);
    // </if>

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: []}},
    };
    flush();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      browser: {enable_spellchecking: {value: false}},
      spellcheck: {
        dictionaries: {value: ['en-US']},
        use_spelling_service: {value: false},
      },
    };
    flush();
    shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.click();
    assertTrue(testElement.prefs.spellcheck.use_spelling_service.value);
  });

  // Spellcheck link is shown on Chrome OS in Browser settings only.
  // <if expr="chromeos_ash">
  test('Spellcheck link', function() {
    loadTimeData.overrideValues({
      isOSSettings: false,
    });
    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: ['en-US']}},
    };
    flush();
    const shadowRoot = testElement.shadowRoot!;
    assertFalse(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      spellcheck: {dictionaries: {value: []}},
    };
    flush();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);
  });
  // </if>
});
