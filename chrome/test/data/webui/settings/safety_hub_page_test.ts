// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {CardInfo, SettingsSafetyHubPageElement} from 'chrome://settings/lazy_load.js';
import {CardState, ContentSettingsTypes, SafeBrowsingSetting, SafetyHubBrowserProxyImpl, SafetyHubEvent} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, LifetimeBrowserProxyImpl, MetricsBrowserProxyImpl, PasswordManagerImpl, PasswordManagerPage, Router, routes, SafetyHubModuleType, SafetyHubSurfaces} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';
import {TestSafetyHubBrowserProxy} from './test_safety_hub_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// <if expr="not chromeos_ash">
import {eventToPromise} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

suite('SafetyHubPage', function() {
  let testElement: SettingsSafetyHubPageElement;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;
  let safetyHubBrowserProxy: TestSafetyHubBrowserProxy;
  let passwordManagerProxy: TestPasswordManagerProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  const notificationPermissionMockData = [{
    origin: 'www.example.com',
    notificationInfoString: 'About 1 notifications a day',
  }];

  const unusedSitePermissionMockData = [{
    origin: 'www.example.com',
    permissions: [ContentSettingsTypes.CAMERA],
    expiration: '13317004800000000',  // Represents 2023-01-01T00:00:00.
  }];

  const passwordCardMockData: CardInfo = {
    header: '2 compromised passwords',
    subheader: 'You should change these now',
    state: CardState.WARNING,
  };

  const versionCardMockData: CardInfo = {
    header: 'Chrome is up to date',
    subheader: 'Checked just now',
    state: CardState.SAFE,
  };

  const safeBrowsingCardMockData: CardInfo = {
    header: 'Safe Browsing is off',
    subheader: 'An Extension turned off Safe Browsing',
    state: CardState.INFO,
  };

  setup(function() {
    safetyHubBrowserProxy = new TestSafetyHubBrowserProxy();
    safetyHubBrowserProxy.setPasswordCardData(passwordCardMockData);
    safetyHubBrowserProxy.setVersionCardData(versionCardMockData);
    safetyHubBrowserProxy.setSafeBrowsingCardData(safeBrowsingCardMockData);
    SafetyHubBrowserProxyImpl.setInstance(safetyHubBrowserProxy);
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);

    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-safety-hub-page');
    testElement.prefs = settingsPrefs.prefs!;
    document.body.appendChild(testElement);
    return flushTasks();
  });

  function assertNoRecommendationState(shouldBeVisible: boolean) {
    assertEquals(
        shouldBeVisible, isChildVisible(testElement, '#emptyStateModule'));
    assertEquals(
        shouldBeVisible, isChildVisible(testElement, '#userEducationModule'));
  }

  async function changeSafeBrowsingGeneratedPref(setting: SafeBrowsingSetting) {
    testElement.set('prefs.generated.safe_browsing', {
      value: setting,
      type: chrome.settingsPrivate.PrefType.DICTIONARY,
    });
    assertEquals(setting, testElement.getPref('generated.safe_browsing').value);
    await flushTasks();
  }

  function assertSafeBrowsingCard(newCardData: CardInfo) {
    assertEquals(
        1, safetyHubBrowserProxy.getCallCount('getSafeBrowsingCardData'));
    assertTrue(isChildVisible(testElement, '#safeBrowsing'));
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#header')!
            .textContent!.trim(),
        newCardData.header);
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#subheader')!
            .textContent!.trim(),
        newCardData.subheader);
  }

  test(
      'No Recommendation State Visibility With Unused Site Permissions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertNoRecommendationState(true);

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(
            SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
            unusedSitePermissionMockData);
        await flushTasks();
        assertNoRecommendationState(false);

        // Once hidden, it remains hidden as other modules are visible.
        webUIListenerCallback(
            SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
        await flushTasks();
        assertNoRecommendationState(false);
      });

  test(
      'No Recommendation State Visibility With Notification Permissions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertNoRecommendationState(true);

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(
            SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
            notificationPermissionMockData);
        await flushTasks();
        assertNoRecommendationState(false);

        // Once hidden, it remains hidden as other modules are visible.
        webUIListenerCallback(
            SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
        await flushTasks();
        assertNoRecommendationState(false);
      });

  test(
      'No Recommendation State Visibility With Extensions Module',
      async function() {
        // The element is visible when there is nothing to review.
        assertTrue(isChildVisible(testElement, '#emptyStateModule'));

        // The element becomes hidden if the is any module that needs attention.
        webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 1);
        await flushTasks();
        assertFalse(isChildVisible(testElement, '#emptyStateModule'));

        // Returns when extension module goes away.
        webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 0);
        await flushTasks();
        assertTrue(isChildVisible(testElement, '#emptyStateModule'));
      });

  test('Unused Site Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    const unusedSitePermissionsElementTag =
        'settings-safety-hub-unused-site-permissions';
    assertFalse(isChildVisible(testElement, unusedSitePermissionsElementTag));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));

    webUIListenerCallback(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        unusedSitePermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, unusedSitePermissionsElementTag));
  });

  test('Notification Permissions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    const notificationPermissionsElementTag =
        'settings-safety-hub-notification-permissions-module';
    assertFalse(isChildVisible(testElement, notificationPermissionsElementTag));

    // The element becomes visible if the list of permissions is no longer
    // empty.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        notificationPermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));

    // Once visible, it remains visible regardless of list length.
    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED, []);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));

    webUIListenerCallback(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        notificationPermissionMockData);
    await flushTasks();
    assertTrue(isChildVisible(testElement, notificationPermissionsElementTag));
  });

  test('Extensions Module Visibility', async function() {
    // The element is not visible when there is nothing to review.
    assertFalse(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));

    // The element becomes visible if the there are extensions to review.
    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 2);
    await flushTasks();
    assertTrue(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));

    // Once visible, it goes away if all extensions are handled.
    webUIListenerCallback(SafetyHubEvent.EXTENSIONS_CHANGED, 0);
    await flushTasks();
    assertFalse(
        isChildVisible(testElement, 'settings-safety-hub-extensions-module'));
  });

  test('Password Card', async function() {
    assertTrue(isChildVisible(testElement, '#passwords'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.passwords.shadowRoot!.querySelector(
                                               '#header')!.textContent!.trim(),
        passwordCardMockData.header);
    assertEquals(
        testElement.$.passwords.shadowRoot!.querySelector('#subheader')!
            .textContent!.trim(),
        passwordCardMockData.subheader);

    // Check that the card aria role and description are correct.
    assertEquals(testElement.$.passwords.getAttribute('role'), 'link');
    assertEquals(
        testElement.$.passwords.getAttribute('aria-description'),
        testElement.i18n('safetyHubPasswordNavigationAriaLabel'));
  });

  test('Password Card Clicked', async function() {
    testElement.$.passwords.click();

    // Ensure the Password Manager Check-up page is shown.
    const param = await passwordManagerProxy.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);

    // Ensure the card state on click metrics are recorded.
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    const result =
        await metricsBrowserProxy.whenCalled('recordSafetyHubCardStateClicked');
    assertEquals('Settings.SafetyHub.PasswordsCard.StatusOnClick', result[0]);
    assertEquals(passwordCardMockData.state, result[1]);
  });

  test('Password Card Clicked via Enter', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    // Ensure the Password Manager Check-up page is shown.
    const param = await passwordManagerProxy.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('Password Card Clicked via Space', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', bubbles: true}));
    // Ensure the Password Manager Check-up page is shown.
    const param = await passwordManagerProxy.whenCalled('showPasswordManager');
    assertEquals(PasswordManagerPage.CHECKUP, param);
  });

  test('Version Card', async function() {
    assertTrue(isChildVisible(testElement, '#version'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.version.shadowRoot!.querySelector(
                                             '#header')!.textContent!.trim(),
        versionCardMockData.header);
    assertEquals(
        testElement.$.version.shadowRoot!.querySelector(
                                             '#subheader')!.textContent!.trim(),
        versionCardMockData.subheader);

    // Check that the card aria role and description are correct.
    assertEquals(testElement.$.passwords.getAttribute('role'), 'link');
    assertEquals(
        testElement.$.version.getAttribute('aria-description'),
        testElement.i18n('safetyHubVersionNavigationAriaLabel'));
  });

  test('Version Card Clicked When No Update Waiting', async function() {
    testElement.$.version.click();

    // Ensure the About page is shown.
    assertEquals(routes.ABOUT, Router.getInstance().getCurrentRoute());

    // Ensure the card state on click metrics are recorded.
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    const result =
        await metricsBrowserProxy.whenCalled('recordSafetyHubCardStateClicked');
    assertEquals('Settings.SafetyHub.VersionCard.StatusOnClick', result[0]);
    assertEquals(versionCardMockData.state, result[1]);
  });

  test('Version Card Clicked When Update Waiting', async function() {
    const versionCardMockData: CardInfo = {
      header: 'Chrome is not up to date',
      subheader: 'Relaunch to update',
      state: CardState.WARNING,
    };
    safetyHubBrowserProxy.setVersionCardData(versionCardMockData);
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    await flushTasks();

    // Check that the card aria role and description are correct.
    assertEquals(testElement.$.version.getAttribute('role'), 'button');
    assertEquals(
        testElement.$.version.getAttribute('aria-description'),
        testElement.i18n('safetyHubVersionRelaunchAriaLabel'));

    // <if expr="not chromeos_ash">
    lifetimeBrowserProxy.setShouldShowRelaunchConfirmationDialog(true);
    lifetimeBrowserProxy.setRelaunchConfirmationDialogDescription(
        'Test description.');
    // </if>

    testElement.$.version.click();

    // <if expr="not chromeos_ash">
    // Ensure the confirmation dialog is always shown.
    await eventToPromise('cr-dialog-open', testElement);
    const relaunchConfirmationDialogElement =
        testElement.shadowRoot!.querySelector('relaunch-confirmation-dialog')!;
    assertTrue(relaunchConfirmationDialogElement.$.dialog.open);

    // Ensure the confirmation dialog shows a correct description.
    const dialog = relaunchConfirmationDialogElement.shadowRoot!.querySelector(
        'cr-dialog')!;
    const description =
        dialog.shadowRoot!.querySelector<HTMLSlotElement>('slot[name=body]')!;
    assertEquals(
        'Test description.', description.assignedNodes()[0]!.textContent);

    relaunchConfirmationDialogElement.$.confirm.click();
    // </if>

    // Ensure the card state on click metrics are recorded.
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    const result =
        await metricsBrowserProxy.whenCalled('recordSafetyHubCardStateClicked');
    assertEquals('Settings.SafetyHub.VersionCard.StatusOnClick', result[0]);
    assertEquals(versionCardMockData.state, result[1]);

    // Ensure the browser is restarted.
    return lifetimeBrowserProxy.whenCalled('relaunch');
  });

  test('Version Card Clicked via Enter', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    // Ensure the About page is shown.
    assertEquals(routes.ABOUT, Router.getInstance().getCurrentRoute());
  });

  test('Version Card Clicked via Space', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', bubbles: true}));
    // Ensure the About page is shown.
    assertEquals(routes.ABOUT, Router.getInstance().getCurrentRoute());
  });

  test('Safe Browsing Card', async function() {
    assertTrue(isChildVisible(testElement, '#safeBrowsing'));

    // Card header and subheader should be what the browser proxy provides.
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#header')!
            .textContent!.trim(),
        safeBrowsingCardMockData.header);
    assertEquals(
        testElement.$.safeBrowsing.shadowRoot!.querySelector('#subheader')!
            .textContent!.trim(),
        safeBrowsingCardMockData.subheader);

    // Check that the card aria role and description are correct.
    assertEquals(testElement.$.passwords.getAttribute('role'), 'link');
    assertEquals(
        testElement.$.safeBrowsing.getAttribute('aria-description'),
        testElement.i18n('safetyHubSBNavigationAriaLabel'));
  });

  test('Safe Browsing Card Clicked', async function() {
    testElement.$.safeBrowsing.click();

    // Ensure the Security Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());

    // Ensure the card state on click metrics are recorded.
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    const result =
        await metricsBrowserProxy.whenCalled('recordSafetyHubCardStateClicked');
    assertEquals(
        'Settings.SafetyHub.SafeBrowsingCard.StatusOnClick', result[0]);
    assertEquals(safeBrowsingCardMockData.state, result[1]);
  });

  test('Safe Browsing Card Clicked via Enter', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter', bubbles: true}));
    // Ensure the Security Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test('Safe Browsing Card Clicked via Space', async function() {
    testElement.$.passwords.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', bubbles: true}));
    // Ensure the Security Settings page is shown.
    assertEquals(routes.SECURITY, Router.getInstance().getCurrentRoute());
  });

  test(
      `Safe Browsing Card updates upon Safe Browsing settings change`,
      async function() {
        const standardCardData: CardInfo = {
          header: 'Safe Browsing is on',
          subheader: 'You are getting a standard protection.',
          state: CardState.SAFE,
        };

        const enhancedCardData: CardInfo = {
          header: 'Enhanced Safe Browsing is on',
          subheader: 'You are getting an enhanced protection.',
          state: CardState.SAFE,
        };

        const disabledCardData: CardInfo = safeBrowsingCardMockData;

        // Set the generated.safe_browsing pref to STANDARD.
        safetyHubBrowserProxy.setSafeBrowsingCardData(standardCardData);
        await changeSafeBrowsingGeneratedPref(SafeBrowsingSetting.STANDARD);

        // Change the generated.safe_browsing pref to ENHANCED and check that it
        // triggers getSafeBrowsingCardData call and UI change.
        safetyHubBrowserProxy.resetResolver('getSafeBrowsingCardData');
        safetyHubBrowserProxy.setSafeBrowsingCardData(enhancedCardData);
        await changeSafeBrowsingGeneratedPref(SafeBrowsingSetting.ENHANCED);
        assertSafeBrowsingCard(enhancedCardData);

        // Change the generated.safe_browsing pref to DISABLED and check that it
        // triggers getSafeBrowsingCardData call and UI change.
        safetyHubBrowserProxy.resetResolver('getSafeBrowsingCardData');
        safetyHubBrowserProxy.setSafeBrowsingCardData(disabledCardData);
        await changeSafeBrowsingGeneratedPref(SafeBrowsingSetting.DISABLED);
        assertSafeBrowsingCard(disabledCardData);

        // Change the generated.safe_browsing pref to STANDARD and check that it
        // triggers getSafeBrowsingCardData call and UI change.
        safetyHubBrowserProxy.resetResolver('getSafeBrowsingCardData');
        safetyHubBrowserProxy.setSafeBrowsingCardData(standardCardData);
        await changeSafeBrowsingGeneratedPref(SafeBrowsingSetting.STANDARD);
        assertSafeBrowsingCard(standardCardData);
      });

  test('Dismiss all menu notifications on page load', async function() {
    Router.getInstance().navigateTo(routes.SAFETY_HUB);
    await safetyHubBrowserProxy.whenCalled('dismissActiveMenuNotification');
  });

  test('Metric Recording for Dashboard State', async function() {
    const safeCardData: CardInfo = {
      header: 'Dummy header',
      subheader: 'Dummy subheader',
      state: CardState.SAFE,
    };

    const unsafeCardData: CardInfo = {
      header: 'Dummy header',
      subheader: 'Dummy subheader',
      state: CardState.WARNING,
    };

    function reset() {
      // Reset all cards and modules on safety hub page.
      safetyHubBrowserProxy.setPasswordCardData(safeCardData);
      safetyHubBrowserProxy.setVersionCardData(safeCardData);
      safetyHubBrowserProxy.setSafeBrowsingCardData(safeCardData);
      safetyHubBrowserProxy.setUnusedSitePermissions([]);
      safetyHubBrowserProxy.setNotificationPermissionReview([]);
      safetyHubBrowserProxy.setNumberOfExtensionsThatNeedReview(0);
      metricsBrowserProxy.reset();
    }

    async function refresh(): Promise<void> {
      // Refresh the page to consume recent mock data.
      document.body.removeChild(testElement);
      testElement = document.createElement('settings-safety-hub-page');
      document.body.appendChild(testElement);
      await flushTasks();
    }

    reset();
    await refresh();
    // Expect recordSafetyHubDashboardAnyWarning is called as false since
    // there is no warning.
    let result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubDashboardAnyWarning');
    assertEquals(false, result);

    // Check general interaction and impression metrics.
    result = await metricsBrowserProxy.whenCalled('recordSafetyHubImpression');
    assertEquals(SafetyHubSurfaces.SAFETY_HUB_PAGE, result);
    result = await metricsBrowserProxy.whenCalled('recordSafetyHubInteraction');
    assertEquals(SafetyHubSurfaces.SAFETY_HUB_PAGE, result);

    // Expect recordSafetyHubModuleWarningImpression is called for password
    // card.
    reset();
    safetyHubBrowserProxy.setPasswordCardData(unsafeCardData);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.PASSWORDS, result);

    // Expect recordSafetyHubModuleWarningImpression is called for version card.
    reset();
    safetyHubBrowserProxy.setVersionCardData(unsafeCardData);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.VERSION, result);

    // Expect recordSafetyHubModuleWarningImpression is called for safe browsing
    // card.
    reset();
    safetyHubBrowserProxy.setSafeBrowsingCardData(unsafeCardData);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.SAFE_BROWSING, result);

    // Expect recordSafetyHubModuleWarningImpression is called for unused site
    // permissions.
    reset();
    safetyHubBrowserProxy.setUnusedSitePermissions(
        unusedSitePermissionMockData);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.PERMISSIONS, result);

    // Expect recordSafetyHubModuleWarningImpression is called for notification
    // permissions.
    reset();
    safetyHubBrowserProxy.setNotificationPermissionReview(
        notificationPermissionMockData);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.NOTIFICATIONS, result);

    // Expect recordSafetyHubModuleWarningImpression is called for extensions.
    reset();
    safetyHubBrowserProxy.setNumberOfExtensionsThatNeedReview(1);
    await refresh();
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubModuleWarningImpression');
    assertEquals(SafetyHubModuleType.EXTENSIONS, result);

    // Expect recordSafetyHubDashboardAnyWarning is called as true.
    result = await metricsBrowserProxy.whenCalled(
        'recordSafetyHubDashboardAnyWarning');
    assertEquals(true, result);
  });

  test('Metric Recording for Education module', async function() {
    assertNoRecommendationState(true);

    const eduModule = testElement.shadowRoot!.querySelector<HTMLElement>(
        '#userEducationModule');
    const links =
        eduModule!.shadowRoot!.querySelectorAll<HTMLAnchorElement>('a');
    assertEquals(3, links.length);

    // Check clicking the Safety Tools link causes metric recording.
    assertTrue(!!links[0]);
    links[0].click();
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    assertEquals(
        'Settings.SafetyHub.SafetyToolsLinkClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.reset();

    // Check clicking the Incognito link causes metric recording.
    assertTrue(!!links[1]);
    links[1].click();
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    assertEquals(
        'Settings.SafetyHub.IncognitoLinkClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
    metricsBrowserProxy.reset();

    // Check clicking the Safe Browsing link causes metric recording.
    assertTrue(!!links[2]);
    links[2].click();
    await safetyHubBrowserProxy.whenCalled('recordSafetyHubInteraction');
    assertEquals(
        'Settings.SafetyHub.SafeBrowsingLinkClicked',
        await metricsBrowserProxy.whenCalled('recordAction'));
  });

  test('Record Safety Hub page visit', async function() {
    // Override setTimeout, and only alter behavior for the 20s timeout (the
    // delay for considering a SH page visit).
    // Using MockTimer did not work here, as it interfered with many other,
    // unrelated timers.
    const origSetTimeout = window.setTimeout;
    window.setTimeout = function(
        handler: TimerHandler, timeout: number|undefined): number {
      if (timeout === 20000) {
        const callback = handler as Function;
        callback();
        return 0;
      }
      return origSetTimeout(handler, timeout);
    };

    document.body.removeChild(testElement);
    testElement = document.createElement('settings-safety-hub-page');
    document.body.appendChild(testElement);
    await flushTasks();

    await safetyHubBrowserProxy.whenCalled('recordSafetyHubPageVisit');

    window.setTimeout = origSetTimeout;
  });
});
