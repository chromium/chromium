// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsPersonalizationOptionsElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefService, PrefsBrowserProxy, PrivacyPageBrowserProxyImpl, resetPageVisibilityForTesting, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
// <if expr="_google_chrome and is_chromeos">
import {OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
// </if>
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
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

// clang-format on

function createBooleanPref(
    name: string, value: boolean): chrome.settingsPrivate.PrefObject {
  return {
    key: name,
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: value,
  };
}

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    createBooleanPref('search.suggest_enabled', true),
    createBooleanPref('url_keyed_anonymized_data_collection.enabled', true),
    createBooleanPref('page_content_collection.enabled', false),
    createBooleanPref('price_tracking.email_notifications_enabled', true),
    createBooleanPref('signin.allowed_on_next_startup', true),
    createBooleanPref('spellcheck.use_spelling_service', false),
    createBooleanPref('browser.enable_spellchecking', true),
    {
      key: 'spellcheck.dictionaries',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: ['en-US'],
    },
  ];
}

suite('AllBuilds', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let syncBrowserProxy: TestSyncBrowserProxy;
  let testElement: SettingsPersonalizationOptionsElement;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      signinAvailable: true,
      changePriceEmailNotificationsEnabled: true,
      shouldUseMetricsConsentRestructure: true,
    });
  });

  function buildTestElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
  }

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

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
    await microtasksFinished();
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));
    const descriptionText =
        testElement.shadowRoot.querySelector(
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
    await microtasksFinished();
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // New response to return should not show.
    const infoResponse_hide = {
      shouldShowSettings: false,
      choice: ChromeSigninUserChoice.NO_CHOICE,
      signedInEmail: '',
    };

    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse_hide);
    await microtasksFinished();
    assertFalse(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // Original response to return should show again.
    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse);
    await microtasksFinished();
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
    await microtasksFinished();
    assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

    // `ChromeSigninUserChoice.NO_CHOICE` leads to no value set.
    assertEquals(
        Number(testElement.$.chromeSigninUserChoiceSelection.value),
        ChromeSigninUserChoice.NO_CHOICE);

    infoResponse.choice = ChromeSigninUserChoice.SIGNIN;
    webUIListenerCallback(
        'chrome-signin-user-choice-info-change', infoResponse);
    await microtasksFinished();
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

        // Rebuild the element simulating a fresh start.
        buildTestElement();
        await syncBrowserProxy.whenCalled('getChromeSigninUserChoiceInfo');
        await microtasksFinished();
        assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));

        // Update user selection
        const menu = testElement.$.chromeSigninUserChoiceSelection;
        menu.value = ChromeSigninUserChoice.SIGNIN.toString();
        menu.dispatchEvent(new CustomEvent('change'));
        await microtasksFinished();

        assertTrue(isVisible(testElement.$.chromeSigninUserChoiceSelection));
        assertTrue(testElement.$.chromeSigninUserChoiceToast.open);
      });

  test('signinAllowedToggle', async function() {
    const toggle = testElement.$.signinAllowedToggle;
    assertTrue(isVisible(toggle));

    testElement.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();
    // Check initial setup.
    assertTrue(toggle.checked);
    assertTrue(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertFalse(testElement.$.toast.open);

    // When the user is signed out, clicking the toggle should work
    // normally and the restart toast should be opened.
    toggle.click();
    await microtasksFinished();
    assertFalse(toggle.checked);
    assertFalse(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertTrue(testElement.$.toast.open);

    // Clicking it again, turns the toggle back on. The toast remains
    // open.
    toggle.click();
    await microtasksFinished();
    assertTrue(toggle.checked);
    assertTrue(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertTrue(testElement.$.toast.open);

    // Reset toast.
    testElement.$.toast.hide();

    // When the user is part way through sync setup, the toggle should be
    // disabled in an on state.
    testElement.syncStatus = {
      firstSetupInProgress: true,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();
    assertTrue(toggle.disabled);
    assertTrue(toggle.checked);

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();
    // When the user is signed in, clicking the toggle should open the
    // sign-out dialog.
    assertFalse(
        !!testElement.shadowRoot.querySelector('settings-signout-dialog'));
    toggle.click();

    await eventToPromise('cr-dialog-open', testElement);
    await microtasksFinished();
    // The toggle remains on.
    assertTrue(toggle.checked);
    assertTrue(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertFalse(testElement.$.toast.open);

    let signoutDialog =
        testElement.shadowRoot.querySelector('settings-signout-dialog');
    assertTrue(!!signoutDialog);
    assertTrue(signoutDialog.$.dialog.open);

    // The user clicks cancel.
    const cancel = signoutDialog.shadowRoot.querySelector<HTMLElement>(
        '#disconnectCancel')!;
    cancel.click();

    await eventToPromise('close', signoutDialog);
    await microtasksFinished();
    assertFalse(
        !!testElement.shadowRoot.querySelector('settings-signout-dialog'));

    // After the dialog is closed, the toggle remains turned on.
    assertTrue(toggle.checked);
    assertTrue(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertFalse(testElement.$.toast.open);

    // The user clicks the toggle again.
    toggle.click();
    await eventToPromise('cr-dialog-open', testElement);
    await microtasksFinished();
    signoutDialog =
        testElement.shadowRoot.querySelector('settings-signout-dialog');
    assertTrue(!!signoutDialog);
    assertTrue(signoutDialog.$.dialog.open);

    // The user clicks confirm, which signs them out.
    const disconnectConfirm =
        signoutDialog.shadowRoot.querySelector<HTMLElement>(
            '#disconnectConfirm')!;
    disconnectConfirm.click();

    await eventToPromise('close', signoutDialog);
    await microtasksFinished();
    // After the dialog is closed, the toggle is turned off and the
    // toast is shown.
    assertFalse(toggle.checked);
    assertFalse(
        prefService.getPref<boolean>('signin.allowed_on_next_startup').value);
    assertTrue(testElement.$.toast.open);
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
        testElement.shadowRoot.querySelector('#searchSuggestToggle')));
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
        testElement.shadowRoot.querySelector('#searchSuggestToggle')));
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
        testElement.shadowRoot.querySelector('#searchSuggestToggle')));
  });
  // </if>

  test('searchAggregatorSuggestNotShown', function() {
    loadTimeData.overrideValues({showSearchAggregatorSuggest: false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.
    assertFalse(isVisible(testElement.shadowRoot.querySelector(
        '#searchAggregatorSuggestToggle')));
  });

  test('searchAggregatorSuggestShown', function() {
    loadTimeData.overrideValues({showSearchAggregatorSuggest: true});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.
    assertTrue(isVisible(testElement.shadowRoot.querySelector(
        '#searchAggregatorSuggestToggle')));
  });

  test('priceEmailNotificationsToggleHidden', async function() {
    loadTimeData.overrideValues(
        {'changePriceEmailNotificationsEnabled': false});
    buildTestElement();  // Rebuild the element after modifying loadTimeData.

    assertFalse(!!testElement.shadowRoot.querySelector(
        '#priceEmailNotificationsToggle'));

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();
    assertFalse(!!testElement.shadowRoot.querySelector(
        '#priceEmailNotificationsToggle'));
  });

  test(
      'priceEmailNotificationsToggleShownForSignedInUsersWithFlag',
      async function() {
        loadTimeData.overrideValues({
          'changePriceEmailNotificationsEnabled': true,
          // Flag is enabled.
          'replaceSyncPromosWithSignInPromos': true,
        });
        // Rebuild the element after modifying loadTimeData.
        buildTestElement();

        testElement.syncStatus = {
          signedInState: SignedInState.SIGNED_IN,
          statusAction: StatusAction.NO_ACTION,
        };
        await microtasksFinished();
        assertTrue(!!testElement.shadowRoot.querySelector(
            '#priceEmailNotificationsToggle'));
      });

  test(
      'priceEmailNotificationsToggleShownForSyncingUsersWithFlag',
      async function() {
        loadTimeData.overrideValues({
          'changePriceEmailNotificationsEnabled': true,
          // Flag is enabled.
          'replaceSyncPromosWithSignInPromos': true,
        });
        // Rebuild the element after modifying loadTimeData.
        buildTestElement();

        testElement.syncStatus = {
          signedInState: SignedInState.SYNCING,
          statusAction: StatusAction.NO_ACTION,
        };
        await microtasksFinished();
        assertTrue(!!testElement.shadowRoot.querySelector(
            '#priceEmailNotificationsToggle'));
      });

  test(
      'priceEmailNotificationsToggleHiddenForSignedInUsersWithoutFlag',
      async function() {
        loadTimeData.overrideValues({
          'changePriceEmailNotificationsEnabled': true,
          // Flag is disabled.
          'replaceSyncPromosWithSignInPromos': false,
        });
        // Rebuild the element after modifying loadTimeData.
        buildTestElement();

        testElement.syncStatus = {
          signedInState: SignedInState.SIGNED_IN,
          statusAction: StatusAction.NO_ACTION,
        };
        await microtasksFinished();
        assertFalse(!!testElement.shadowRoot.querySelector(
            '#priceEmailNotificationsToggle'));
      });

  test(
      'priceEmailNotificationsToggleShownForSyncingUsersWithoutFlag',
      async function() {
        loadTimeData.overrideValues({
          'changePriceEmailNotificationsEnabled': true,
          // Flag is disabled.
          'replaceSyncPromosWithSignInPromos': false,
        });
        // Rebuild the element after modifying loadTimeData.
        buildTestElement();

        testElement.syncStatus = {
          signedInState: SignedInState.SYNCING,
          statusAction: StatusAction.NO_ACTION,
        };
        await microtasksFinished();
        assertTrue(!!testElement.shadowRoot.querySelector(
            '#priceEmailNotificationsToggle'));
      });
});

// <if expr="_google_chrome">
suite('OfficialBuild', function() {
  let testBrowserProxy: TestPrivacyPageBrowserProxy;
  let testElement: SettingsPersonalizationOptionsElement;
  let prefService: PrefService;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      signinAvailable: true,
      changePriceEmailNotificationsEnabled: true,
      shouldUseMetricsConsentRestructure: true,
    });
  });

  function buildTestElement() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
  }

  setup(async function() {
    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testBrowserProxy = new TestPrivacyPageBrowserProxy();
    PrivacyPageBrowserProxyImpl.setInstance(testBrowserProxy);
    buildTestElement();
  });

  teardown(function() {
    testElement.remove();
  });

  // On ChromeOS, the spellcheck toggle is in OS Settings, not browser
  // settings. TODO (https://www.crbug.com/1396704): Add this test in the OS
  // settings test for the OS version of personalization options, once OS
  // Settings supports TypeScript tests.
  // <if expr="not is_chromeos">
  test('Spellcheck toggle', async function() {
    prefService.setPrefValue('spellcheck.dictionaries', ['en-US']);
    await microtasksFinished();
    const shadowRoot = testElement.shadowRoot;
    assertFalse(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);

    prefService.setPrefValue('spellcheck.dictionaries', []);
    await microtasksFinished();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.hidden);

    prefService.setPrefValue('spellcheck.dictionaries', ['en-US']);
    prefService.setPrefValue('spellcheck.use_spelling_service', false);
    await microtasksFinished();
    shadowRoot.querySelector<HTMLElement>('#spellCheckControl')!.click();
    await microtasksFinished();
    assertTrue(
        prefService.getPref<boolean>('spellcheck.use_spelling_service').value);
  });
  // </if>

  // Only the spellcheck link is shown on Chrome OS in Browser settings.
  // <if expr="is_chromeos">
  test('Spellcheck link', async function() {
    prefService.setPrefValue('spellcheck.dictionaries', ['en-US']);
    await microtasksFinished();
    const shadowRoot = testElement.shadowRoot;
    assertFalse(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);

    prefService.setPrefValue('spellcheck.dictionaries', []);
    await microtasksFinished();
    assertTrue(
        shadowRoot.querySelector<HTMLElement>('#spellCheckLink')!.hidden);
  });

  test(
      'Metrics row hidden when metrics consent restructure is enabled',
      function() {
        assertFalse(isChildVisible(testElement, '#metricsReportingLink'));
      });

  test(
      'Metrics row links to OS Settings Privacy Hub subpage', async function() {
        const openWindowProxy = new TestOpenWindowProxy();
        OpenWindowProxyImpl.setInstance(openWindowProxy);

        loadTimeData.overrideValues(
            {shouldUseMetricsConsentRestructure: false});
        buildTestElement();

        assertTrue(isChildVisible(testElement, '#metricsReportingLink'));

        testElement.shadowRoot
            .querySelector<HTMLElement>('#metricsReportingLink')!.click();
        const url = await openWindowProxy.whenCalled('openUrl');
        const expectedUrl =
            loadTimeData.getString('osSettingsPrivacyHubSubpageUrl');
        assertEquals(expectedUrl, url);
      });
  // </if>
});
// </if>
