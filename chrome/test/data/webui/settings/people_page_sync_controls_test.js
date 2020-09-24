// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Router, StatusAction,SyncBrowserProxyImpl} from 'chrome://settings/settings.js';
import {getSyncAllPrefs, setupRouterWithSyncRoutes} from 'chrome://test/settings/sync_test_util.m.js';
import {TestSyncBrowserProxy} from 'chrome://test/settings/test_sync_browser_proxy.m.js';
import {waitBeforeNextRender} from 'chrome://test/test_util.m.js';

// clang-format on

suite('SyncControlsTest', function() {
  let syncControls = null;
  let browserProxy = null;
  let syncEverything = null;
  let customizeSync = null;

  setup(function() {
    setupRouterWithSyncRoutes();
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    syncControls = document.createElement('settings-sync-controls');
    document.body.appendChild(syncControls);

    // Start with Sync All.
    webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
    flush();

    return waitBeforeNextRender().then(() => {
      syncEverything =
          syncControls.$$('cr-radio-button[name="sync-everything"]');
      customizeSync = syncControls.$$('cr-radio-button[name="customize-sync"]');
      assertTrue(!!syncEverything);
      assertTrue(!!customizeSync);
    });
  });

  teardown(function() {
    syncControls.remove();
  });


  /**
   * @param {!settings.SyncPrefs} syncPrefs
   * @param {NodeList<!CrToggleElement>} datatypeControls
   */
  function assertPrefs(prefs, datatypeControls) {
    const expected = getSyncAllPrefs();
    expected.syncAllDataTypes = false;
    assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

    webUIListenerCallback('sync-prefs-changed', expected);

    // Assert that all the individual datatype controls are enabled.
    for (const control of datatypeControls) {
      assertFalse(control.disabled);
      assertTrue(control.checked);
    }
    browserProxy.resetResolver('setSyncDatatypes');
  }

  test('SettingIndividualDatatypes', function() {
    assertTrue(syncEverything.checked);
    assertFalse(customizeSync.checked);
    assertEquals(syncControls.$$('#syncAllDataTypesControl'), null);

    // Assert that all the individual datatype controls are disabled.
    const datatypeControls = syncControls.shadowRoot.querySelectorAll(
        '.list-item:not([hidden]) > cr-toggle');

    assertTrue(datatypeControls.length > 0);
    for (const control of datatypeControls) {
      assertTrue(control.disabled);
      assertTrue(control.checked);
    }

    customizeSync.click();
    flush();
    assertFalse(syncEverything.checked);
    assertTrue(customizeSync.checked);

    return browserProxy.whenCalled('setSyncDatatypes')
        .then(prefs => assertPrefs(prefs, datatypeControls));
  });

  test('SignedIn', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls
        .syncStatus = {disabled: false, hasError: false, signedIn: true};
    // Controls are available when signed in and there is no error.
    assertFalse(syncControls.hidden);
  });

  test('SyncDisabled', function() {
    syncControls.syncStatus = {disabled: true, hasError: false, signedIn: true};
    // Controls are hidden when sync is disabled.
    assertTrue(syncControls.hidden);
  });

  test('SyncError', function() {
    syncControls.syncStatus = {disabled: false, hasError: true, signedIn: true};
    // Controls are hidden when there is an error but it's not a
    // passphrase error.
    assertTrue(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.ENTER_PASSPHRASE
    };
    // Controls are available when there is a passphrase error.
    assertFalse(syncControls.hidden);
  });
});

suite('SyncControlsSubpageTest', function() {
  let syncControls = null;
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    syncControls = document.createElement('settings-sync-controls');
    const router = Router.getInstance();
    router.navigateTo(router.getRoutes().SYNC_ADVANCED);
    document.body.appendChild(syncControls);

    syncControls
        .syncStatus = {disabled: false, hasError: false, signedIn: true};
    flush();

    assertEquals(router.getRoutes().SYNC_ADVANCED, router.getCurrentRoute());
  });

  teardown(function() {
    syncControls.remove();
  });

  test('SignedOut', function() {
    syncControls
        .syncStatus = {disabled: false, hasError: false, signedIn: false};
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC.path, router.getCurrentRoute().path);
  });

  test('PassphraseError', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.ENTER_PASSPHRASE
    };
    const router = Router.getInstance();
    assertEquals(
        router.getRoutes().SYNC_ADVANCED.path, router.getCurrentRoute().path);
  });

  test('SyncPaused', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.REAUTHENTICATE
    };
    const router = Router.getInstance();
    assertEquals(router.getRoutes().SYNC.path, router.getCurrentRoute().path);
  });
});
