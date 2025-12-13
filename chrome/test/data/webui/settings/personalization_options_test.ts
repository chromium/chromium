// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsPersonalizationOptionsElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PrivacyPageBrowserProxyImpl, resetPageVisibilityForTesting, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// <if expr="_google_chrome or not is_chromeos">
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
// </if>

// <if expr="not is_chromeos">
import {eventToPromise} from 'chrome://webui-test/test_util.js';
import {ChromeSigninUserChoice} from 'chrome://settings/settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';

// </if>

import {TestPrivacyPageBrowserProxy} from './test_privacy_page_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('AllBuilds', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testElement: SettingsPersonalizationOptionsElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      signinAvailable: true,
      changePriceEmailNotificationsEnabled: true,
    });
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function buildTestElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-personalization-options');
    testElement.prefs = settingsPrefs.prefs!;
    testElement.set('prefs.page_content_collection.enabled.value', false);
    document.body.appendChild(testElement);
    flush();
  }

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    syncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncBrowserProxy);
    buildTestElement();
  });

  teardown(function() {
    testElement.remove();
    resetPageVisibilityForTesting();
  });

  // <if expr="not is_chromeos">
  test('chromeSigninUserChoiceAvailableInitialization', async function() {
    assertFalse(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    const infoResponse = {
      shouldShowSettings: true,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: 'test@gmail.com',
    };
    syncBrowserProxy.setGetUserChromeSigninUserChoiceInfoResponse(infoResponse);

    buildTestElement();  // Rebuild the element simulating a fresh start.
    await syncBrowserProxy.whenCalled('getChromeSigninUserChoiceInfo');
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));
    const descriptionText =
        testElement.shadowRoot!.querySelector(
                                   '#chromeSigninChoiceDescription')!.innerHTML;
    assertTrue(descriptionText.includes(infoResponse.signedInEmail));
  });

  test('chromeSigninUserChoiceAvailabilityUpdate', async function() {
    const infoResponse = {
      shouldShowSettings: true,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: 'test@gmail.com',
    };
    syncBrowserProxy.setGetUserChromeSigninUserChoiceInfoResponse(infoResponse);

    buildTestElement();  // Rebuild the element simulating a fresh start.
    await syncBrowserProxy.whenCalled('getChromeSigninUserChoiceInfo');
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // New response to return should not show.
    const infoResponse_hide = {
      shouldShowSettings: false,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: '',
    };

    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse_hide);
    assertFalse(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // Original response to return should show again.
    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse);
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));
  });

  test('chromeSigninUserChoiceUpdatedExternally', async function() {
    const infoResponse = {
      shouldShowSettings: true,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: 'test@gmail.com',
    };
    syncBrowserProxy.setGetUserChromeSigninUserChoiceInfoResponse(infoResponse);

    buildTestElement();  // Rebuild the element simulating a fresh start.
    await syncBrowserProxy.whenCalled('getChromeSigninUserChoiceInfo');
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // `ChromeSigninUserChoice.NO_CHOICE` leads to no value set.
    assertEquals(
        Number(testElement.$.chromeSigninUserChoiceSelection.value),
        ChromeSigninUserChoice.NO_CHOICE);

    infoResponse.choice = ChromeSigninUserChoice.SIGNIN;
    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse);
    assertEquals(
        Number(testElement.$.chromeSigninUserChoiceSelection.value),
        ChromeSigninUserChoice.SIGNIN);
  });

  test(
      'chromeSigninUserChoiceAvailabilityUpdateWithSnackbarEnabled',
      async function() {
        const infoResponse = {
          shouldShowSettings: true,
          choice: ChromeSigninUserChoice.ALWAYS_ASK,
          signedInEmail: 'test@gmail.com',
        };
        syncBrowserProxy.setGetUserChromeSigninUserChoiceInfoResponse(
            infoResponse);

        buildTestElement();  // Rebuild the element simulating a fresh start.
        await syncBrowserProxy.whenCalled('getChromeSigninUserChoiceInfo');
        assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

        // Update user selection
        const menu = testElement.$.chromeSigninUserChoiceSelection;
        menu.value = ChromeSigninUserChoice.SIGNIN.toString();
        menu.dispatchEvent(new CustomEvent('change'));
        flush();

        assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));
        assertTrue(testElement.$.chromeSigninUserChoiceToast.open);
      });

  test('signinAllowedToggle', function() {
    const toggle = testElement.$.signinAllowedToggle;
    assertTrue(isVisible(toggle));

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
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
      signedInState: SignedInState.SYNCING,
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
          assertTrue(signoutDialog.$.dialog.open);

          // The user clicks cancel.
          const cancel = signoutDialog.shadowRoot!.querySelector<HTMLElement>(
              '#disconnectCancel')!;
          cancel.click();

          return eventToPromise('close', signoutDialog);
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
          assertTrue(signoutDialog.$.dialog.open);

          // The user clicks confirm, which signs them out.
          const disconnectConfirm =
              signoutDialog.shadowRoot!.querySelector<HTMLElement>(
                  '#disconnectConfirm')!;
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

  test('searchSuggestToggleShownIfPageVisibilityUndefined', function() {
    // This is the most common case, as in non-Guest profiles on Desktop
    // platforms pageVisibility is undefined.
    assertTrue(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });

  test('searchSuggestToggleHiddenByPageVisibility', function() {
    resetPageVisibilityForTesting({
      privacy: {
        searchPrediction: false,
        networkPrediction: false,
      },
    });
    buildTestElement();
    assertFalse(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });

  test('searchSuggestToggleShownByPageVisibility', function() {
    resetPageVisibilityForTesting({
      privacy: {
        searchPrediction: true,
        networkPrediction: false,
      },
    });
    buildTestElement();
    assertTrue(isVisible(
        testElement.shadowRoot!.querySelector('#searchSuggestToggle')));
  });
  // </if>

  test('searchAggregatorSuggestNotShown', function() {
    loadTimeData.overrideValues({showSearchAggregatorSuggest: false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.
    assertFalse(isVisible(
      testElement.shadowRoot!.querySelector('#searchAggregatorSuggestToggle')));
  });

  test('searchAggregatorSuggestShown', function() {
    loadTimeData.overrideValues({showSearchAggregatorSuggest: true});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.
    assertTrue(isVisible(
      testElement.shadowRoot!.querySelector('#searchAggregatorSuggestToggle')));
  });

  test('priceEmailNotificationsToggleHidden', function() {
    loadTimeData.overrideValues(
        {'changePriceEmailNotificationsEnabled': false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test('priceEmailNotificationsToggleShownForSignedInUsersWithFlag', function() {
    loadTimeData.overrideValues({
      'changePriceEmailNotificationsEnabled': true,
      // Flag is enabled.
      'replaceSyncPromosWithSignInPromos': true,
    });
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test('priceEmailNotificationsToggleShownForSyncingUsersWithFlag', function() {
    loadTimeData.overrideValues({
      'changePriceEmailNotificationsEnabled': true,
      // Flag is enabled.
      'replaceSyncPromosWithSignInPromos': true,
    });
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test('priceEmailNotificationsToggleHiddenForSignedInUsersWithoutFlag', function() {
    loadTimeData.overrideValues({
      'changePriceEmailNotificationsEnabled': true,
      // Flag is disabled.
      'replaceSyncPromosWithSignInPromos': false,
    });
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertFalse(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test('priceEmailNotificationsToggleShownForSyncingUsersWithoutFlag', function() {
    loadTimeData.overrideValues({
      'changePriceEmailNotificationsEnabled': true,
      // Flag is disabled.
      'replaceSyncPromosWithSignInPromos': false,
    });
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(!!testElement.shadowRoot!.querySelector(
        '#priceEmailNotificationsToggle'));
  });
});

// <if expr="_google_chrome">
suite('OfficialBuild', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsPersonalizationOptionsElement;

  setup(function() {
    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
  });

  teardown(function() {
    testElement.remove();
  });

  // On ChromeOS, the spellcheck toggle is in OS Settings, not browser
  // settings. TODO (https://www.crbug.com/1396704): Add this test in the OS
  // settings test for the OS version of personalization options, once OS
  // Settings supports TypeScript tests.
  // <if expr="not is_chromeos">
  test('Spellcheck toggle', function() {
    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      page_content_collection: {enabled: {value: true}},
      spellcheck: {dictionaries: {value: ['en-US']}},
    };
    flush();
    const shadowRoot = testElement.shadowRoot!;
    assertFalse(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      page_content_collection: {enabled: {value: true}},
      spellcheck: {dictionaries: {value: []}},
    };
    flush();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);

    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      page_content_collection: {enabled: {value: true}},
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
  // </if>

  // Only the spellcheck link is shown on Chrome OS in Browser settings.
  // <if expr="is_chromeos">
  test('Spellcheck link', function() {
    testElement.prefs = {
      profile: {password_manager_leak_detection: {value: true}},
      safebrowsing:
          {enabled: {value: true}, scout_reporting_enabled: {value: true}},
      page_content_collection: {enabled: {value: true}},
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
      page_content_collection: {enabled: {value: true}},
      spellcheck: {dictionaries: {value: []}},
    };
    flush();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);
  });
  // </if>

  // <if expr="is_chromeos">
  test('Metrics row links to OS Settings Privacy Hub subpage', function() {
    let targetUrl: string = '';
    testElement['navigateTo_'] = (url: string) => {
      targetUrl = url;
    };

    testElement.$.metricsReportingLink.click();
    const expectedUrl =
        loadTimeData.getString('osSettingsPrivacyHubSubpageUrl');
    assertEquals(expectedUrl, targetUrl);
  });
  // </if>
});
// </if>
