// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SettingsSyncControlsElement} from 'chrome://settings/lazy_load.js';
import {CrRadioButtonElement, CrToggleElement, Router, StatusAction, SyncBrowserProxyImpl, SyncPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {getSyncAllPrefs, setupRouterWithSyncRoutes, SyncRoutes} from './sync_test_util.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

// clang-format on

suite('SyncControlsTest', async function() {
  let syncControls: SettingsSyncControlsElement;
  let browserProxy: TestSyncBrowserProxy;
  let syncEverything: CrRadioButtonElement;
  let customizeSync: CrRadioButtonElement;

  setup(async function() {
    setupRouterWithSyncRoutes();
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
    assertTrue(!!syncEverything);
    assertTrue(!!customizeSync);
  });

  teardown(function() {
    syncControls.remove();
  });


  function assertPrefs(
      prefs: SyncPrefs, datatypeControls: NodeListOf<CrToggleElement>) {
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
    flush();
    assertFalse(syncEverything.checked);
    assertTrue(customizeSync.checked);

    const prefs = await browserProxy.whenCalled('setSyncDatatypes');
    assertPrefs(prefs, datatypeControls);
  });

  test('SignedIn', function() {
    // Controls are available by default.
    assertFalse(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are available when signed in and there is no error.
    assertFalse(syncControls.hidden);
  });

  test('SyncDisabled', function() {
    syncControls.syncStatus = {
      disabled: true,
      hasError: false,
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when sync is disabled.
    assertTrue(syncControls.hidden);
  });

  test('SyncError', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    // Controls are hidden when there is an error but it's not a
    // passphrase error.
    assertTrue(syncControls.hidden);

    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    // Controls are available when there is a passphrase error.
    assertFalse(syncControls.hidden);
  });
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
    router.navigateTo((router.getRoutes() as SyncRoutes).SYNC_ADVANCED);
    document.body.appendChild(syncControls);

    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedIn: true,
      statusAction: StatusAction.NO_ACTION,
    };
    flush();

    assertEquals(
        (router.getRoutes() as SyncRoutes).SYNC_ADVANCED,
        router.getCurrentRoute());
  });

  teardown(function() {
    syncControls.remove();
  });

  test('SignedOut', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: false,
      signedIn: false,
      statusAction: StatusAction.NO_ACTION,
    };
    const router = Router.getInstance();
    assertEquals(
        (router.getRoutes() as SyncRoutes).SYNC.path,
        router.getCurrentRoute().path);
  });

  test('PassphraseError', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.ENTER_PASSPHRASE,
    };
    const router = Router.getInstance();
    assertEquals(
        (router.getRoutes() as SyncRoutes).SYNC_ADVANCED.path,
        router.getCurrentRoute().path);
  });

  test('SyncPaused', function() {
    syncControls.syncStatus = {
      disabled: false,
      hasError: true,
      signedIn: true,
      statusAction: StatusAction.REAUTHENTICATE,
    };
    const router = Router.getInstance();
    assertEquals(
        (router.getRoutes() as SyncRoutes).SYNC.path,
        router.getCurrentRoute().path);
  });
});
