// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OsSyncBrowserProxyImpl, Router, routes, StatusAction} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {OsSyncBrowserProxy} */
class TestOsSyncBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'didNavigateToOsSyncPage',
      'didNavigateAwayFromOsSyncPage',
      'setOsSyncDatatypes',
    ]);
  }

  /** @override */
  didNavigateToOsSyncPage() {
    this.methodCalled('didNavigateToOsSyncPage');
  }

  /** @override */
  didNavigateAwayFromOsSyncPage() {
    this.methodCalled('didNavigateAwayFromSyncPage');
  }

  /** @override */
  setOsSyncDatatypes(osSyncPrefs) {
    this.methodCalled('setOsSyncDatatypes', osSyncPrefs);
  }
}

/**
 * Returns a sync prefs dictionary with either all or nothing syncing.
 * @param {boolean} syncAll
 * @return {!OsSyncPrefs}
 */
function getOsSyncPrefs(syncAll) {
  return {
    osAppsRegistered: true,
    osAppsSynced: syncAll,
    osPreferencesRegistered: true,
    osPreferencesSynced: syncAll,
    syncAllOsTypes: syncAll,
    wallpaperEnabled: syncAll,
    wifiConfigurationsRegistered: true,
    wifiConfigurationsSynced: syncAll,
  };
}

function getSyncAllPrefs() {
  return getOsSyncPrefs(true);
}

function getSyncNothingPrefs() {
  return getOsSyncPrefs(false);
}

// Returns a SyncStatus representing the default syncing state.
function getDefaultSyncStatus() {
  return {
    disabled: false,
    hasError: false,
    hasUnrecoverableError: false,
    signedIn: true,
    statusAction: StatusAction.NO_ACTION,
  };
}

function setupSync() {
  webUIListenerCallback('os-sync-prefs-changed', getSyncAllPrefs());
  flush();
}

suite('OsSyncControlsTest', function() {
  let browserProxy = null;
  let syncControls = null;
  let syncEverything = null;
  let customizeSync = null;

  setup(function() {
    browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();
    syncControls = document.createElement('os-sync-controls');
    document.body.appendChild(syncControls);

    syncEverything = syncControls.shadowRoot.querySelector(
        'cr-radio-button[name="sync-everything"]');
    customizeSync = syncControls.shadowRoot.querySelector(
        'cr-radio-button[name="customize-sync"]');
    assertTrue(!!syncEverything);
    assertTrue(!!customizeSync);
  });

  teardown(function() {
    syncControls.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('ControlsHiddenUntilInitialUpdateSent', function() {
    assertTrue(syncControls.hidden);
    setupSync();
    assertFalse(syncControls.hidden);
  });

  test('SyncEnabled', function() {
    setupSync();

    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);

    const labels = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > div.checkbox-label');
    for (const label of labels) {
      assertFalse(label.hasAttribute('label-disabled'));
    }

    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('UncheckingSyncAllEnablesAllIndividualControls', async function() {
    setupSync();
    customizeSync.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('PrefChangeUpdatesControls', function() {
    const prefs = getSyncAllPrefs();
    prefs.syncAllOsTypes = false;
    webUIListenerCallback('os-sync-prefs-changed', prefs);

    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('DisablingOneControlUpdatesPrefs', async function() {
    setupSync();

    // Select "Customize sync" instead of "Sync everything".
    customizeSync.click();
    // Disable "Apps".
    syncControls.$.osAppsControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osAppsSynced = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('DisablingSettingsAlsoDisablesWallpaper', async function() {
    setupSync();

    // Select "Customize sync" instead of "Sync everything".
    customizeSync.click();
    // Disable "Settings".
    syncControls.$.osPreferencesControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osPreferencesSynced = false;
    expectedPrefs.wallpaperEnabled = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });
});

suite('OsSyncControlsNavigationTest', function() {
  test('DidNavigateEvents', async function() {
    const browserProxy = new TestOsSyncBrowserProxy();
    OsSyncBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.OS_SYNC);
    await browserProxy.methodCalled('didNavigateToOsSyncPage');

    Router.getInstance().navigateTo(routes.OS_PEOPLE);
    await browserProxy.methodCalled('didNavigateAwayFromOsSyncPage');
  });
});
