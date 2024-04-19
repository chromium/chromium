// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSyncBrowserProxyImpl, OsSyncControlsSubpageElement, OsSyncPrefs} from 'chrome://os-settings/lazy_load.js';
import {CrRadioButtonElement, CrToggleElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestOsSyncBrowserProxy} from './test_os_sync_browser_proxy.js';

suite('<os-sync-controls-subpage>', () => {
  let browserProxy: TestOsSyncBrowserProxy;
  let syncControls: OsSyncControlsSubpageElement;
  let syncEverything: CrRadioButtonElement;
  let customizeSync: CrRadioButtonElement;

  /**
   * Returns a sync prefs dictionary with either all or nothing syncing.
   */
  function getOsSyncPrefs(syncAll: boolean): OsSyncPrefs {
    return {
      osAppsRegistered: true,
      osAppsSynced: syncAll,
      osPreferencesRegistered: true,
      osPreferencesSynced: syncAll,
      syncAllOsTypes: syncAll,
      wallpaperEnabled: syncAll,
      osWifiConfigurationsRegistered: true,
      osWifiConfigurationsSynced: syncAll,
    };
  }

  function getSyncAllPrefs(): OsSyncPrefs {
    return getOsSyncPrefs(true);
  }

  function setupSync(): void {
    webUIListenerCallback('os-sync-prefs-changed', getSyncAllPrefs());
    flush();
  }

  setup(() => {
    browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);

    syncControls = document.createElement('os-sync-controls-subpage');
    document.body.appendChild(syncControls);

    const syncBtn =
        syncControls.shadowRoot!.querySelector<CrRadioButtonElement>(
            'cr-radio-button[name="sync-everything"]');
    assertTrue(!!syncBtn);
    syncEverything = syncBtn;

    const customizeBtn =
        syncControls.shadowRoot!.querySelector<CrRadioButtonElement>(
            'cr-radio-button[name="customize-sync"]');
    assertTrue(!!customizeBtn);
    customizeSync = customizeBtn;
  });

  teardown(() => {
    syncControls.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  test('ControlsHiddenUntilInitialUpdateSent', () => {
    assertTrue(syncControls.hidden);
    setupSync();
    assertFalse(syncControls.hidden);
  });

  test('SyncEnabled', () => {
    setupSync();

    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);

    const labels = syncControls.shadowRoot!.querySelectorAll(
        '.list-item:not([hidden]) > div.checkbox-label');
    for (const label of labels) {
      assertFalse(label.hasAttribute('label-disabled'));
    }

    const datatypeControls =
        syncControls.shadowRoot!.querySelectorAll<CrToggleElement>(
            '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('UncheckingSyncAllEnablesAllIndividualControls', async () => {
    setupSync();
    customizeSync.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('PrefChangeUpdatesControls', () => {
    const prefs = getSyncAllPrefs();
    prefs.syncAllOsTypes = false;
    webUIListenerCallback('os-sync-prefs-changed', prefs);

    const datatypeControls =
        syncControls.shadowRoot!.querySelectorAll<CrToggleElement>(
            '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('DisablingOneControlUpdatesPrefs', async () => {
    setupSync();

    // Select "Customize sync" instead of "Sync everything".
    customizeSync.click();
    // Disable "Apps".
    const toggle = syncControls.shadowRoot!.querySelector<CrToggleElement>(
        '#osAppsControl');
    assertTrue(!!toggle);
    toggle.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osAppsSynced = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('DisablingSettingsAlsoDisablesWallpaper', async () => {
    setupSync();

    // Select "Customize sync" instead of "Sync everything".
    customizeSync.click();
    // Disable "Settings".
    const toggle = syncControls.shadowRoot!.querySelector<CrToggleElement>(
        '#osPreferencesControl');
    assertTrue(!!toggle);
    toggle.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osPreferencesSynced = false;
    expectedPrefs.wallpaperEnabled = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });
});

suite('OsSyncControlsNavigationTest', () => {
  test('DidNavigateEvents', async () => {
    const browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.OS_SYNC);
    await browserProxy.methodCalled('didNavigateToOsSyncPage');

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    await browserProxy.methodCalled('didNavigateAwayFromOsSyncPage');
  });
});
