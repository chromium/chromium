// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.OsSyncBrowserProxy} */
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
 * Returns a sync prefs dictionary with everything set to sync.
 * @return {!settings.OsSyncPrefs}
 */
function getSyncAllOsPrefs() {
  return {
    osPreferencesEnforced: false,
    osPreferencesRegistered: true,
    osPreferencesSynced: true,
    printersEnforced: false,
    printersRegistered: true,
    printersSynced: true,
    syncAllOsTypes: true,
  };
}

suite('OsSyncControlsTest', function() {
  let syncControls = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestOsSyncBrowserProxy();
    settings.OsSyncBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    syncControls = document.createElement('os-sync-controls');
    document.body.appendChild(syncControls);

    // Start with Sync All.
    cr.webUIListenerCallback('os-sync-prefs-changed', getSyncAllOsPrefs());
    Polymer.dom.flush();
  });

  teardown(function() {
    syncControls.remove();
  });

  test('SyncAllEnabledByDefault', function() {
    const syncAllControl = syncControls.$.syncAllOsTypesControl;
    assertFalse(syncAllControl.disabled);
    assertTrue(syncAllControl.checked);
  });

  test('IndividualControlsDisabledByDefault', async function() {
    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');

    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('UncheckingSyncAllEnablesAllIndividualControls', async function() {
    syncControls.$.syncAllOsTypesControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllOsPrefs();
    expectedPrefs.syncAllOsTypes = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('PrefChangeUpdatesControls', function() {
    const prefs = getSyncAllOsPrefs();
    prefs.syncAllOsTypes = false;
    cr.webUIListenerCallback('os-sync-prefs-changed', prefs);

    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');
    for (const control of datatypeControls) {
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }
  });

  test('DisablingOneControlUpdatesPrefs', async function() {
    // Disable "Sync All".
    syncControls.$.syncAllOsTypesControl.click();
    // Disable "Settings".
    syncControls.$.osPreferencesControl.click();
    const prefs = await browserProxy.whenCalled('setOsSyncDatatypes');

    const expectedPrefs = getSyncAllOsPrefs();
    expectedPrefs.syncAllOsTypes = false;
    expectedPrefs.osPreferencesSynced = false;
    assertEquals(JSON.stringify(expectedPrefs), JSON.stringify(prefs));
  });

  test('ControlsVisibleByDefault', function() {
    assertFalse(syncControls.hidden);
  });

  test('ControlsVisibleWhenNoError', function() {
    syncControls
        .syncStatus = {disabled: false, hasError: false, signedIn: true};
    assertFalse(syncControls.hidden);
  });

  test('ControlsHiddenWhenSyncDisabled', function() {
    syncControls.syncStatus = {disabled: true, hasError: false, signedIn: true};
    assertTrue(syncControls.hidden);
  });

  test('ControlsHiddenWhenSyncHasError', function() {
    syncControls.syncStatus = {disabled: false, hasError: true, signedIn: true};
    assertTrue(syncControls.hidden);
  });
});

suite('OsSyncControlsNavigationTest', function() {
  test('DidNavigateEvents', async function() {
    const browserProxy = new TestOsSyncBrowserProxy();
    settings.OsSyncBrowserProxyImpl.instance_ = browserProxy;

    settings.navigateTo(settings.routes.OS_SYNC);
    await browserProxy.methodCalled('didNavigateToOsSyncPage');

    settings.navigateTo(settings.routes.PEOPLE);
    await browserProxy.methodCalled('didNavigateAwayFromOsSyncPage');
  });
});
