// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrCollapseElement, CrExpandButtonElement, SettingsAccountPageElement, SettingsSyncEncryptionOptionsElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, OpenWindowProxyImpl, resetRouterForTesting, Router, routes, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {getSyncAllPrefs, simulateStoredAccounts, simulateSyncStatus} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';


suite('AccountPage', function() {
  let accountSettingsPage: SettingsAccountPageElement;
  let testSyncBrowserProxy: TestSyncBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;
  let encryptionElement: SettingsSyncEncryptionOptionsElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues(
        {replaceSyncPromosWithSignInPromos: true, isEeaChoiceCountry: false});
    resetRouterForTesting();

    testSyncBrowserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(testSyncBrowserProxy);

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    accountSettingsPage = createSettingsAccountPageElement();
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    Router.getInstance().navigateTo(routes.ACCOUNT);

    await waitBeforeNextRender(accountSettingsPage);
    encryptionElement = accountSettingsPage.shadowRoot!.querySelector(
        'settings-sync-encryption-options')!;
    assertTrue(!!encryptionElement, 'encryptionElement');

    await testSyncBrowserProxy.whenCalled('getStoredAccounts');
    simulateStoredAccounts([{email: 'foo@foo.com'}]);
    flush();

    return microtasksFinished();
  });

  function createSettingsAccountPageElement(): SettingsAccountPageElement {
    const element = document.createElement('settings-account-page');
    testSyncBrowserProxy.testSyncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    document.body.appendChild(element);
    return element;
  }

  async function assertElementLinksToUrl(element: string, url: string) {
    const linkRow =
        accountSettingsPage.shadowRoot!.querySelector<HTMLElement>(element);
    assertTrue(!!linkRow);
    linkRow.click();
    await flushTasks();
    const openedUrl = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString(url), openedUrl);
  }

  // Tests that all elements are visible.
  test('ShowCorrectRows', function() {
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());

    assertTrue(
        isChildVisible(accountSettingsPage, 'settings-sync-account-control'));
    assertTrue(isChildVisible(accountSettingsPage, 'settings-sync-controls'));
    assertTrue(isChildVisible(accountSettingsPage, '#syncDashboardLink'));
    assertTrue(isChildVisible(accountSettingsPage, '#manage-google-account'));
    assertTrue(
        isChildVisible(accountSettingsPage, '#activityControlsLinkRowV2'));
    assertTrue(isChildVisible(accountSettingsPage, '#encryptionDescription'));
    assertFalse(isChildVisible(accountSettingsPage, '#encryptionCollapse'));
  });

  // Tests that we navigate back to the people page if the user is not signed
  // in.
  test('AccountSettingsPageUnavailableWhenNotSignedIn', async function() {
    webUIListenerCallback('sync-status-changed', {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    });
    await microtasksFinished();

    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });

  test('RowsLinkToCorrectUrls', function() {
    assertElementLinksToUrl('#syncDashboardLink', 'syncDashboardUrl');
    assertElementLinksToUrl('#manage-google-account', 'googleAccountUrl');
    assertElementLinksToUrl(
        '#activityControlsLinkRowV2', 'activityControlsUrl');
  });

  // Tests the Advanced Sync Settings
  test('EncryptionExpandButton', async function() {
    const encryptionDescription =
        accountSettingsPage.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#encryptionDescription');
    const encryptionCollapse =
        accountSettingsPage.shadowRoot!.querySelector<CrCollapseElement>(
            '#encryptionCollapse');
    assertTrue(!!encryptionDescription);
    assertTrue(!!encryptionCollapse);

    // No encryption with custom passphrase.
    assertFalse(encryptionCollapse.opened);
    encryptionDescription.click();
    await encryptionDescription.updateComplete;
    assertTrue(encryptionCollapse.opened);

    // Push sync prefs with |prefs.encryptAllData| unchanged. The encryption
    // menu should not collapse.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    flush();
    assertTrue(encryptionCollapse.opened);

    encryptionDescription.click();
    await encryptionDescription.updateComplete;
    assertFalse(encryptionCollapse.opened);

    // Data encrypted with custom passphrase.
    // The encryption menu should be expanded.
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    webUIListenerCallback('sync-prefs-changed', prefs);
    flush();
    assertTrue(encryptionCollapse.opened);

    // Clicking |reset Sync| does not change the expansion state.
    const link =
        encryptionDescription.querySelector<HTMLAnchorElement>('a[href]');
    assertTrue(!!link);
    link.target = '';
    link.href = '#';
    // Prevent the link from triggering a page navigation when tapped.
    // Breaks the test in Vulcanized mode.
    link.addEventListener('click', e => e.preventDefault());
    link.click();
    assertTrue(encryptionCollapse.opened);
  });

  test('RadioBoxesHiddenWhenPassphraseRequired', function() {
    const prefs = getSyncAllPrefs();
    prefs.encryptAllData = true;
    prefs.passphraseRequired = true;
    webUIListenerCallback('sync-prefs-changed', prefs);

    flush();

    assertTrue(
        accountSettingsPage.shadowRoot!
            .querySelector<HTMLElement>('#encryptionDescription')!.hidden);
    assertEquals(
        encryptionElement.shadowRoot!
            .querySelector<HTMLElement>(
                '#encryptionRadioGroupContainer')!.style.display,
        'none');
  });

  test('EEAChoiceCountry', async function() {
    loadTimeData.overrideValues({
      isEeaChoiceCountry: true,
    });
    resetRouterForTesting();
    accountSettingsPage = createSettingsAccountPageElement();
    Router.getInstance().navigateTo(routes.ACCOUNT);
    await waitBeforeNextRender(accountSettingsPage);

    assertFalse(
        isChildVisible(accountSettingsPage, '#activityControlsLinkRowV2'));
    assertTrue(
        isChildVisible(accountSettingsPage, '#personalizationExpandButton'));

    // The personalization section is collapsed by default.
    const personalizationCollapse =
        accountSettingsPage.shadowRoot!.querySelector<CrCollapseElement>(
            '#personalizationCollapse');
    assertTrue(!!personalizationCollapse);
    assertFalse(personalizationCollapse.opened);

    // Clicking the expand-button expands the collapse.
    const expandButton =
        accountSettingsPage.shadowRoot!.querySelector<HTMLElement>(
            '#personalizationExpandButton');
    assertTrue(!!expandButton);
    expandButton.click();
    await flushTasks();
    assertTrue(personalizationCollapse.opened);

    // Clicking the expand-button again collapses the collapse.
    expandButton.click();
    await flushTasks();
    assertFalse(personalizationCollapse.opened);

    // The linkedServices row is only visible when the collapse is expanded.
    expandButton.click();
    await flushTasks();

    const linkedServicesLinkRow =
        accountSettingsPage.shadowRoot!.querySelector<HTMLElement>(
            '#linkedServicesLinkRow');
    assertTrue(!!linkedServicesLinkRow);
    linkedServicesLinkRow.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(loadTimeData.getString('linkedServicesUrl'), url);
  });

  // The sync dashboard is not accessible by supervised
  // users, so it should remain hidden.
  test('SyncDashboardHiddenFromSupervisedUsers', async function() {
    const dashboardLink =
        accountSettingsPage.shadowRoot!.querySelector<HTMLElement>(
            '#syncDashboardLink')!;

    const prefs = getSyncAllPrefs();
    webUIListenerCallback('sync-prefs-changed', prefs);

    // Normal user
    assertFalse(dashboardLink.hidden);

    // Supervised user
    await testSyncBrowserProxy.whenCalled('getSyncStatus');
    simulateSyncStatus({
      signedInState: SignedInState.SIGNED_IN,
      supervisedUser: true,
      statusAction: StatusAction.NO_ACTION,
    });
    await microtasksFinished();
    assertTrue(dashboardLink.hidden);
  });
});
