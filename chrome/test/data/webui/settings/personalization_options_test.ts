// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsPersonalizationOptionsElement} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, PrivacyPageVisibility, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, PrivacyPageBrowserProxyImpl, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';
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
  let customPageVisibility: PrivacyPageVisibility;
  let testElement: SettingsPersonalizationOptionsElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      // TODO(crbug.com/40274151): Remove the tests for "driveSuggest" when
      // the setting is completely removed.
      driveSuggestAvailable: true,
      driveSuggestNoSetting: false,
      driveSuggestNoSyncRequirement: false,
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
    testElement.pageVisibility = customPageVisibility;
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
  });

  test('DriveSearchSuggestControl', function() {
    assertFalse(isChildVisible(testElement, '#driveSuggestControl'));

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(isChildVisible(testElement, '#driveSuggestControl'));

    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    flush();
    assertFalse(isChildVisible(testElement, '#driveSuggestControl'));
  });

  test('DriveSearchSuggestControlDeprecated', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();
    assertTrue(isChildVisible(testElement, '#driveSuggestControl'));

    loadTimeData.overrideValues({'driveSuggestNoSetting': false});
    buildTestElement();

    assertFalse(isChildVisible(testElement, '#driveSuggestControl'));
  });

  test('DriveSearchSuggestControlNoSyncRequirement', function() {
    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    flush();
    assertFalse(isChildVisible(testElement, '#driveSuggestControl'));

    loadTimeData.overrideValues({'driveSuggestNoSyncRequirement': true});
    buildTestElement();
    testElement.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    flush();

    assertTrue(isChildVisible(testElement, '#driveSuggestControl'));
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

  test('pageContentRow', function() {
    const pageContentRow =
        testElement.shadowRoot!.querySelector<HTMLElement>('#pageContentRow')!;

    // TODO(crbug.com/40070860): Remove visibility check once crbug/1476887
    // launched.
    assertTrue(isVisible(pageContentRow));

    // The sublabel is dynamic based on the setting state.
    testElement.set('prefs.page_content_collection.enabled.value', true);
    const row = testElement.shadowRoot!.querySelector<CrLinkRowElement>(
        '#pageContentRow')!;
    assertEquals(
        loadTimeData.getString('pageContentLinkRowSublabelOn'), row.subLabel);
    testElement.set('prefs.page_content_collection.enabled.value', false);
    assertEquals(
        loadTimeData.getString('pageContentLinkRowSublabelOff'), row.subLabel);

    // A click on the row navigates to the page content page.
    pageContentRow.click();
    assertEquals(routes.PAGE_CONTENT, Router.getInstance().getCurrentRoute());
  });

  test('historySearchRow', () => {
    loadTimeData.overrideValues({
      showHistorySearchControl: true,
      enableAiSettingsPageRefresh: false,
    });
    resetRouterForTesting();
    buildTestElement();

    const historySearchRow =
        testElement.shadowRoot!.querySelector<HTMLElement>('#historySearchRow');
    assertTrue(!!historySearchRow);
    assertTrue(isVisible(historySearchRow));
    historySearchRow.click();
    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.HISTORY_SEARCH, currentRoute);
    assertEquals(routes.SYNC, currentRoute.parent);

    loadTimeData.overrideValues({showHistorySearchControl: false});
    buildTestElement();
    assertFalse(!!testElement.shadowRoot!.querySelector<HTMLElement>(
        '#historySearchRow'));
  });
});

// TODO(crbug.com/40070860): Remove once crbug/1476887 launched.
suite('PageContentSettingOff', function() {
  let testElement: SettingsPersonalizationOptionsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enablePageContentSetting: false,
    });
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-personalization-options');
    document.body.appendChild(testElement);
    flush();
  });

  teardown(function() {
    testElement.remove();
  });

  test('pageContentRowNotVisible', function() {
    assertFalse(
        isVisible(testElement.shadowRoot!.querySelector('#pageContentRow')));
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

  // On ChromeOS Ash, the spellcheck toggle is in OS Settings, not browser
  // settings. TODO (https://www.crbug.com/1396704): Add this test in the OS
  // settings test for the OS version of personalization options, once OS
  // Settings supports TypeScript tests.
  // <if expr="not chromeos_ash">
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
  // <if expr="chromeos_ash">
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

  // <if expr="chromeos_ash">
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
