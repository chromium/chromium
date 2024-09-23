// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSyncControlsElement} from 'chrome://settings/lazy_load.js';
import type {CrLinkRowElement, CrRadioButtonElement, CrToggleElement, SyncPrefs} from 'chrome://settings/settings.js';
import {Router, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {getSyncAllPrefs, getSyncAllPrefsManaged} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// <if expr="chromeos_lacros">
import {loadTimeData} from 'chrome://settings/settings.js';
// </if>

// clang-format on

suite('SyncControlsTest', async function() {
  let syncControls: SettingsSyncControlsElement;
  let browserProxy: TestSyncBrowserProxy;
  let syncEverything: CrRadioButtonElement;
  let customizeSync: CrRadioButtonElement;
  let radioGroup: HTMLElement;

  setup(async function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);

    // Start with Sync All.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    flush();

    await waitBeforeNextRender(syncControls);
    syncEverything = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="sync-everything"]')!;
    customizeSync = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="customize-sync"]')!;
    const group = syncControls.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!group);
    radioGroup = group;
    assertTrue(!!customizeSync);
    assertTrue(!!radioGroup);
  });

  function assertPrefs(
      prefs: SyncPrefs, datatypeControls: NodeListOf<CrToggleElement>) {
    const expected = getSyncAllPrefs();
    expected.syncAllDataTypes = false;
    assertDeepEquals(expected, prefs);

    webUIListenerCallback('sync-prefs-changed', expected);

    // Assert that all the individual datatype controls are checked and enabled.
    for (const control of datatypeControls) {
      // if lacros we check that Apps sync toggle is disabled when
      // kSyncChromeOSAppsToggleSharing feature is enabled.
      // <if expr="chromeos_lacros">
      if (control.id === 'appsSyncToggle') {
        const showSyncSettingsRevamp =
            loadTimeData.getBoolean('showSyncSettingsRevamp');
        assertEquals(control.disabled, showSyncSettingsRevamp);
        assertTrue(control.checked);
        continue;
      }
      // </if>
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }

    // Assert that all policy indicators are hidden.
    const policyIndicators =
        syncControls.shadowRoot!.querySelectorAll('cr-policy-indicator');
    assertTrue(policyIndicators.length > 0);
    for (const indicator of policyIndicators) {
      assertFalse(isVisible(indicator));
    }

    browserProxy.resetResolver('setSyncDatatypes');
  }

  test('SettingIndividualDatatypes', async function() {
    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);
    assertEquals(
        syncControls.shadowRoot!.querySelector('#syncAllDataTypesControl'),
        null);

    // Assert that all the individual datatype controls are disabled.
    const datatypeControls =
        syncControls.shadowRoot!.querySelectorAll<CrToggleElement>(
            '.list-item:not([hidden]) > cr-toggle');

    assertTrue(datatypeControls.length > 0);
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }

    customizeSync.click();
    await eventToPromise('selected-changed', radioGroup);
    assertFalse(syncEverything.checked);
    assertTrue(customizeSync.checked);

    const prefs = await browserProxy.whenCalled('setSyncDatatypes');
    assertPrefs(prefs, datatypeControls);
  });

  test('OsSyncSettingsLink', function() {
    // Check that the os sync settings link
    // is available only in Lacros and kSyncChromeOSAppsToggleSharing
    // feature is enabled.
    const osSyncSettingsLink: CrLinkRowElement =
        syncControls.shadowRoot!.querySelector('#osSyncSettingsLink')!;

    // <if expr="chromeos_lacros">
    assertTrue(!!osSyncSettingsLink);
    const showSyncSettingsRevamp =
        loadTimeData.getBoolean('showSyncSettingsRevamp');
    assertEquals(osSyncSettingsLink.hidden, !showSyncSettingsRevamp);
    // </if>

    // <if expr="not chromeos_lacros">
    assertFalse(!!osSyncSettingsLink);
    // </if>
  });

  test('SignedIn', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are available when signed in and there is no error.
    assertFalse(syncControls.hidden);
  });

  test('SyncDisabled', function() {
    syncControls.syncStatus = {
      disabled: true,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when sync is disabled.
    assertTrue(syncControls.hidden);
  });

  test('SyncError', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when there is an error but it's not a
    // passphrase error.
    assertTrue(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    // Controls are available when there is a passphrase error.
    assertFalse(syncControls.hidden);
  });

  // <if expr="chromeos_ash">
  test('SyncCookiesSupported', async function() {
    // Sync everything enabled.
    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);

    // The cookies element is not visible when syncCookiesSupported is disabled
    // (default).
    let cookieListItem = syncControls.shadowRoot!.querySelector(
        '#cookiesSyncItem:not([hidden])');
    assertFalse(!!cookieListItem);

    // Enable syncCookiesSupported.
    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
      syncCookiesSupported: true,
    };
    // The cookies element is now visible.
    cookieListItem = syncControls.shadowRoot!.querySelector(
        '#cookiesSyncItem:not([hidden])');
    assertTrue(!!cookieListItem);
    // Cookies checkbox is disabled.
    let cookiesCheckbox: CrToggleElement =
        syncControls.shadowRoot!.querySelector('#cookiesCheckbox')!;
    assertTrue(!!cookiesCheckbox);
    assertTrue(cookiesCheckbox.disabled);
    assertTrue(cookiesCheckbox.checked);

    // Customize sync enabled.
    customizeSync.click();
    await eventToPromise('selected-changed', radioGroup);
    assertFalse(syncEverything.checked);
    assertTrue(customizeSync.checked);

    // Cookies checkbox is enabled.
    cookiesCheckbox =
        syncControls.shadowRoot!.querySelector('#cookiesCheckbox')!;
    assertTrue(!!cookiesCheckbox);
    assertFalse(cookiesCheckbox.disabled);
    assertTrue(cookiesCheckbox.checked);
  });
  // </if>
});

suite('SyncControlsSubpageTest', function() {
  let syncControls: SettingsSyncControlsElement;
  let browserProxy: TestSyncBrowserProxy;

  setup(function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    syncControls = document.createElement('settings-sync-controls');
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC_ADVANCED);
    document.body.appendChild(syncControls);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();

    assertEquals(router.getRoutes().SYNC_ADVANCED, router.getCurrentRoute());
  });

  test('SignedOut', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC.path, router.getCurrentRoute().path);
  });

  test('PassphraseError', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    const router = Router.getInstance();
    assertEquals(
        router.getRoutes().SYNC_ADVANCED.path, router.getCurrentRoute().path);
  });

  test('SyncPaused', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC.path, router.getCurrentRoute().path);
  });
});

// Test to check that toggles are disabled when sync types are managed by
// policy.
suite('SyncControlsManagedTest', async function() {
  let syncControls: SettingsSyncControlsElement;
  let browserProxy: TestSyncBrowserProxy;
  let syncEverything: CrRadioButtonElement;
  let customizeSync: CrRadioButtonElement;
  let radioGroup: HTMLElement;

  setup(async function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);

    // Start with all prefs managed.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefsManaged());
    // Enable Cookie sync.
    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
      syncCookiesSupported: true,
    };
    flush();

    await waitBeforeNextRender(syncControls);
    syncEverything = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="sync-everything"]')!;
    customizeSync = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="customize-sync"]')!;
    const group = syncControls.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!group);
    radioGroup = group;
    assertTrue(!!syncEverything);
    assertTrue(!!customizeSync);
  });

  test('SettingIndividualDatatypesManaged', async function() {
    // The syncEverything and customizeSync buttons should not be affected by
    // the managed state.
    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);

    const datatypeControls =
        syncControls.shadowRoot!.querySelectorAll<CrToggleElement>(
            '.list-item:not([hidden]) > cr-toggle');
    assertTrue(datatypeControls.length > 0);

    // Assert that all toggles have the policy indicator icon visible when they
    // are all managed.
    const policyIndicators =
        syncControls.shadowRoot!.querySelectorAll('cr-policy-indicator');
    assertTrue(policyIndicators.length > 0);
    for (const indicator of policyIndicators) {
      // On Lacros, if kSyncChromeOSAppsToggleSharing is enabled, syncing of Apps
      // is controlled via the OS, not the browser. So in that case, the policy
      // indicator should not be visible here.
      // <if expr="chromeos_lacros">
      if (indicator.id === 'appsSyncPolicyIndicator') {
        const showSyncSettingsRevamp =
            loadTimeData.getBoolean('showSyncSettingsRevamp');
        assertEquals(isVisible(indicator), !showSyncSettingsRevamp);
        continue;
      }
      // </if>
      assertTrue(isVisible(indicator));
    }

    // Assert that all the individual datatype controls are disabled and
    // unchecked.
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertFalse(control.checked);
    }

    customizeSync.click();
    await eventToPromise('selected-changed', radioGroup);
    assertFalse(syncEverything.checked);
    assertTrue(customizeSync.checked);

    const prefs = await browserProxy.whenCalled('setSyncDatatypes');

    const expected = getSyncAllPrefsManaged();
    expected.syncAllDataTypes = false;
    assertDeepEquals(expected, prefs);

    webUIListenerCallback('sync-prefs-changed', expected);

    // Assert that all the individual datatype controls are still unchecked and
    // disabled.
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertFalse(control.checked);
    }
    browserProxy.resetResolver('setSyncDatatypes');
  });
});

suite('AutofillAndPaymentsToggles', async function() {
  let autofillCheckbox: CrToggleElement;
  let paymentsCheckbox: CrToggleElement;

  function updateComplete(): Promise<void> {
    return Promise
        .all([
          autofillCheckbox.updateComplete,
          paymentsCheckbox.updateComplete,
        ])
        .then(() => {});
  }

  setup(async function() {
    const browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);

    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    flush();

    await waitBeforeNextRender(syncControls);
    const customizeSync: CrRadioButtonElement =
        syncControls.shadowRoot!.querySelector(
            'cr-radio-button[name="customize-sync"]')!;
    const radioGroup = syncControls.shadowRoot!.querySelector('cr-radio-group');
    autofillCheckbox =
        syncControls.shadowRoot!.querySelector('#autofillCheckbox')!;
    paymentsCheckbox =
        syncControls.shadowRoot!.querySelector('#paymentsCheckbox')!;
    assertTrue(!!customizeSync);
    assertTrue(!!radioGroup);
    assertTrue(!!autofillCheckbox);
    assertTrue(!!paymentsCheckbox);

    customizeSync.click();
    await eventToPromise('selected-changed', radioGroup);
    assertTrue(customizeSync.checked);
    assertTrue(autofillCheckbox.checked);
    assertTrue(paymentsCheckbox.checked);
  });

  // Before crbug.com/40265120, the autofill and payments toggles used to be
  // coupled. This test verifies they no longer are.
  test('DecoupledAutofillPaymentsToggles', async function() {
    // Disable Autofill sync.
    autofillCheckbox.click();
    await updateComplete();
    assertFalse(autofillCheckbox.checked);
    assertTrue(paymentsCheckbox.checked);
    assertFalse(paymentsCheckbox.disabled);

    // Disable Payment methods sync.
    paymentsCheckbox.click();
    await updateComplete();
    assertFalse(autofillCheckbox.checked);
    assertFalse(paymentsCheckbox.checked);
    assertFalse(paymentsCheckbox.disabled);

    // Enable Autofill sync.
    autofillCheckbox.click();
    await updateComplete();
    assertTrue(autofillCheckbox.checked);
    assertFalse(paymentsCheckbox.checked);
    assertFalse(paymentsCheckbox.disabled);
  });
});
