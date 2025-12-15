// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsSyncControlsElement} from 'chrome://settings/lazy_load.js';
import type {CrRadioButtonElement, CrToggleElement, SyncPrefs} from 'chrome://settings/settings.js';
import {loadTimeData, Router, resetRouterForTesting, SignedInState, StatusAction, SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {getSyncAllPrefs, getSyncAllPrefsManaged} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// <if expr="not is_chromeos">
import {PageStatus, routes, UserSelectableType} from 'chrome://settings/settings.js';
import {waitAfterNextRender, flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// </if>

// clang-format on

suite('SyncControlsTest', function() {
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
    syncControls.syncStatus = {
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
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

  test('Syncing', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are available when syncing and there is no error.
    assertFalse(syncControls.hidden);
  });

  test('SignedIn', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when signed in, even if there is no error.
    assertTrue(syncControls.hidden);
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

  // Regression test for crbug.com/467318495.
  test('SyncNotConfirmed', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SYNCING,
      statusAction: StatusAction.CONFIRM_SYNC_SETTINGS,
    };
    // Controls are not hidden when sync is not yet confirmed.
    assertFalse(syncControls.hidden);
  });

  // <if expr="is_chromeos">
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

    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: false});
    resetRouterForTesting();

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

  // <if expr="not is_chromeos">
  test(
      'NavigateToAccountSettingsWhenReplacingWithSigninPromoAndNotSyncing',
      function() {
        loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});
        resetRouterForTesting();
        const router = Router.getInstance();
        router.navigateTo(routes.SYNC_ADVANCED);

        syncControls.syncStatus = {
          disabled: false,
          hasError: false,
          signedInState: SignedInState.SIGNED_IN,
          statusAction: StatusAction.NO_ACTION,
        };
        flush();

        assertEquals(routes.ACCOUNT, router.getCurrentRoute());
      });
  // </if>
});

// <if expr="not is_chromeos">
suite('SyncControlsAccountSettingsTest', function() {
  let syncControls: SettingsSyncControlsElement;
  let browserProxy: TestSyncBrowserProxy;

  setup(async function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});
    resetRouterForTesting();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const router = Router.getInstance();
    router.navigateTo(routes.ACCOUNT);
    syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    await waitBeforeNextRender(syncControls);

    assertEquals(routes.ACCOUNT, router.getCurrentRoute());
    await browserProxy.whenCalled('didNavigateToAccountSettingsPage');
  });

  teardown(function() {
    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: false});
    resetRouterForTesting();
  });

  async function setupPrefs() {
    const initialPrefs = getSyncAllPrefs();
    initialPrefs.syncAllDataTypes = false;
    webUIListenerCallback('sync-prefs-changed', initialPrefs);
    await flushTasks();
    await waitAfterNextRender(syncControls);
  }

  function assertControlsEnabled(enabled: boolean) {
    const datatypeControls =
        syncControls.shadowRoot!.querySelectorAll<CrToggleElement>(
            '.list-item:not([hidden]) > cr-toggle');
    assertTrue(datatypeControls.length > 0);
    for (const control of datatypeControls) {
      assertEquals(!enabled, control.disabled);
    }
  }

  function assertSyncDisabledPolicyIndicatorShown(shown: boolean) {
    const policyIndicator = syncControls.shadowRoot!.querySelector<Element>(
        '#syncDisabledIndicator');
    assertEquals(shown, isVisible(policyIndicator));
  }

  function assertIndividualItemPolicyIndicatorsShown(shown: boolean) {
    const policyIndicators = syncControls.shadowRoot!.querySelectorAll(
        'cr-policy-indicator:not(#syncDisabledIndicator)');
    assertTrue(policyIndicators.length > 0);

    // We expect the indicators of the toggles for history, tabs, and saved tab
    // groups to be always hidden since they are merged into one toggle.
    const hiddenIndicators = shown ? 3 : policyIndicators.length;
    let countHiddenIndicators = 0;
    for (const indicator of policyIndicators) {
      if (!isVisible(indicator)) {
        countHiddenIndicators++;
      }
    }

    assertEquals(hiddenIndicators, countHiddenIndicators);
  }

  test('SyncEverythingControlsAreHidden', function() {
    const radioGroup = syncControls.shadowRoot!.querySelector('cr-radio-group');
    const syncEverything = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="sync-everything"]')!;
    const customizeSync = syncControls.shadowRoot!.querySelector(
        'cr-radio-button[name="customize-sync"]')!;

    assertFalse(isVisible(radioGroup));
    assertFalse(isVisible(syncEverything));
    assertFalse(isVisible(customizeSync));
  });

  test('SignedIn', function() {
    setupPrefs();

    // Controls are shown when signed in and there is no error.
    assertFalse(syncControls.hidden);

    // Controls are also not disabled.
    assertControlsEnabled(true);
  });

  test('SignedInError', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when signed in and there is an error.
    assertTrue(syncControls.hidden);
  });

  test('SignedInPassphraseError', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    // Controls are hidden when signed in and there is a passphrase error.
    assertTrue(syncControls.hidden);
  });

  test('SignedInLocalSyncEnabled', async function() {
    setupPrefs();

    // Controls are available by default.
    assertFalse(syncControls.hidden);

    const syncPrefs = getSyncAllPrefs();
    syncPrefs.localSyncEnabled = true;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    await flushTasks();
    await waitAfterNextRender(syncControls);

    // Controls are hidden when signed in and local sync is enabled.
    assertTrue(syncControls.hidden);
  });

  test('ChangeDataTypeToggle', async function() {
    setupPrefs();

    // Make sure that the autofill toggle is present and can be interacted with.
    const autofillToggle =
        syncControls.shadowRoot!.querySelector<CrToggleElement>(
            '#autofillCheckbox');
    assertTrue(!!autofillToggle);
    assertFalse(autofillToggle.disabled);
    assertTrue(autofillToggle.checked);

    // Click to disable autofill sync.
    autofillToggle.click();
    await autofillToggle.updateComplete;

    let [pref, value] = await browserProxy.whenCalled('setSyncDatatype');
    assertEquals(pref, UserSelectableType.AUTOFILL);
    assertFalse(value);
    assertFalse(autofillToggle.checked);

    // Re-enable autofill sync.
    browserProxy.resetResolver('setSyncDatatype');
    autofillToggle.click();
    await autofillToggle.updateComplete;

    [pref, value] = await browserProxy.whenCalled('setSyncDatatype');
    assertEquals(pref, UserSelectableType.AUTOFILL);
    assertTrue(value);
    assertTrue(autofillToggle.checked);
  });

  test(
      'DisableMergedToggleAndShowPolicyIndicatorWhenHistoryAndTabsManaged',
      async () => {
        // Set history and tabs to managed.
        const syncPrefs = getSyncAllPrefs();
        syncPrefs.typedUrlsManaged = true;
        syncPrefs.tabsManaged = true;
        webUIListenerCallback('sync-prefs-changed', syncPrefs);
        await flushTasks();
        await waitAfterNextRender(syncControls);

        // The merged toggle is disabled, but checked because the types are
        // enabled.
        const mergedHistoryTabsToggle =
            syncControls.shadowRoot!.querySelector<CrToggleElement>(
                '#mergedHistoryTabsToggle');
        assertTrue(!!mergedHistoryTabsToggle);
        assertTrue(mergedHistoryTabsToggle.disabled);
        assertTrue(mergedHistoryTabsToggle.checked);

        // Assert that the merged toggle's policy indicator is shown.
        const policyIndicator = syncControls.shadowRoot!.querySelector<Element>(
            '#mergedHistoryTabsToggleIndicator');
        assertTrue(isVisible(policyIndicator));
      });

  test(
      'EnableMergedToggleAndHidePolicyIndicatorWhenOnlyOneDataTypeManaged',
      async () => {
        // Set only history to managed. Tabs remain not managed.
        const syncPrefs = getSyncAllPrefs();
        syncPrefs.typedUrlsManaged = true;
        webUIListenerCallback('sync-prefs-changed', syncPrefs);
        await flushTasks();
        await waitAfterNextRender(syncControls);

        // The merged toggle is not disabled, and checked because the types are
        // enabled.
        const mergedHistoryTabsToggle =
            syncControls.shadowRoot!.querySelector<CrToggleElement>(
                '#mergedHistoryTabsToggle');
        assertTrue(!!mergedHistoryTabsToggle);
        assertFalse(mergedHistoryTabsToggle.disabled);
        assertTrue(mergedHistoryTabsToggle.checked);

        // Assert that the merged toggle's policy indicator is not shown.
        const policyIndicator = syncControls.shadowRoot!.querySelector<Element>(
            '#mergedHistoryTabsToggleIndicator');
        assertFalse(isVisible(policyIndicator));
      });

  test('ChangeMergedHistoryTabsToggle', async function() {
    // Initially, only history is enabled. Tabs and saved tab groups are not.
    const syncPrefs = getSyncAllPrefs();
    syncPrefs.tabsSynced = false;
    syncPrefs.savedTabGroupsSynced = false;
    webUIListenerCallback('sync-prefs-changed', syncPrefs);
    await flushTasks();
    await waitAfterNextRender(syncControls);

    // Override `setSyncDatatype()` in order to collect calls.
    const originalSetSyncDatatype = browserProxy.setSyncDatatype;
    type SyncDatatypeCallArgs = [UserSelectableType, boolean];
    let callsMade: SyncDatatypeCallArgs[] = [];
    browserProxy.setSyncDatatype = function(
        pref: UserSelectableType, value: boolean): Promise<PageStatus> {
      callsMade.push([pref, value]);
      return Promise.resolve(PageStatus.DONE);
    };

    // Make sure that the merged history and tabs toggle is present and can  be
    // interacted with. The toggle is checked, since at least one of the data
    // types is enabled.
    const mergedHistoryTabsToggle =
        syncControls.shadowRoot!.querySelector<CrToggleElement>(
            '#mergedHistoryTabsToggle');
    assertTrue(!!mergedHistoryTabsToggle);
    assertFalse(mergedHistoryTabsToggle.disabled);
    assertTrue(mergedHistoryTabsToggle.checked);

    assertTrue(syncControls.syncPrefs!.typedUrlsSynced);
    assertFalse(syncControls.syncPrefs!.tabsSynced);
    assertFalse(syncControls.syncPrefs!.savedTabGroupsSynced);

    // Click to disable history and tabs.
    mergedHistoryTabsToggle.click();
    await mergedHistoryTabsToggle.updateComplete;

    assertEquals(3, callsMade.length);
    assertTrue(!!callsMade.find(
        ([pref, value]) =>
            pref === UserSelectableType.HISTORY && value === false));
    assertTrue(!!callsMade.find(
        ([pref, value]) =>
            pref === UserSelectableType.TABS && value === false));
    assertTrue(!!callsMade.find(
        ([pref, value]) =>
            pref === UserSelectableType.SAVED_TAB_GROUPS && value === false));

    assertFalse(mergedHistoryTabsToggle.checked);
    callsMade = [];

    // Re-enable history and tabs.
    mergedHistoryTabsToggle.click();
    await mergedHistoryTabsToggle.updateComplete;

    assertEquals(3, callsMade.length);
    assertTrue(!!callsMade.find(
        ([pref, value]) =>
            pref === UserSelectableType.HISTORY && value === true));
    assertTrue(!!callsMade.find(
        ([pref, value]) => pref === UserSelectableType.TABS && value === true));
    assertTrue(!!callsMade.find(
        ([pref, value]) =>
            pref === UserSelectableType.SAVED_TAB_GROUPS && value === true));

    assertTrue(mergedHistoryTabsToggle.checked);

    browserProxy.setSyncDatatype = originalSetSyncDatatype;
  });

  test(
      'DisableToggleAndHidePolicyIndicatorWhenSyncPrefsNotLoaded', async () => {
        webUIListenerCallback('sync-prefs-changed', undefined);
        await flushTasks();
        await waitAfterNextRender(syncControls);

        // Controls are still available when prefs are not loaded.
        assertFalse(syncControls.hidden);

        // However, they are disabled.
        assertControlsEnabled(false);

        // Assert that all policy indicators are hidden.
        assertSyncDisabledPolicyIndicatorShown(false);
        assertIndividualItemPolicyIndicatorsShown(false);
      });

  test('DisableToggleAndHidePolicyIndicatorWhenSyncIsDisabled', async () => {
    setupPrefs();

    syncControls.syncStatus = {
      disabled: true,
      hasError: false,
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    await waitAfterNextRender(syncControls);

    // Controls are still available when sync is disabled.
    assertFalse(syncControls.hidden);

    // However, they are disabled.
    assertControlsEnabled(false);

    // Assert that only the sync disabled policy indicator is shown.
    assertSyncDisabledPolicyIndicatorShown(true);
    assertIndividualItemPolicyIndicatorsShown(false);
  });

  test('DisableToggleAndShowPolicyIndicatorWhenDataTypeIsManaged', async () => {
    // Set all prefs to managed.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefsManaged());
    await flushTasks();
    await waitAfterNextRender(syncControls);

    // Controls are still available when data types are managed.
    assertFalse(syncControls.hidden);

    // However, they are disabled.
    assertControlsEnabled(false);

    // Assert that only individual items' policy indicators are shown.
    assertSyncDisabledPolicyIndicatorShown(false);
    assertIndividualItemPolicyIndicatorsShown(true);
  });

  // Before crbug.com/433895051, the toggles were not shown before a syncStatus
  // update or refresh when the user navigated to the account settings page from
  // a different settings page. This test verifies they are shown directly upon
  // navigation.
  test('TogglesVisibilityUpdatedUponNavigation', async () => {
    const router = Router.getInstance();
    router.navigateTo(routes.PEOPLE);

    // Create the sync controls before navigating to the account settings page.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);
    syncControls.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    await waitAfterNextRender(syncControls);

    router.navigateTo(routes.ACCOUNT);
    assertFalse(syncControls.hidden);
  });
});
// </if>

// Test to check that toggles are disabled when sync types are managed by
// policy.
suite('SyncControlsManagedTest', function() {
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
    const policyIndicators = syncControls.shadowRoot!.querySelectorAll(
        'cr-policy-indicator:not(#syncDisabledIndicator):' +
        'not(#mergedHistoryTabsToggleIndicator)');
    assertTrue(policyIndicators.length > 0);
    for (const indicator of policyIndicators) {
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

suite('AutofillAndPaymentsToggles', function() {
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
    syncControls.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
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
