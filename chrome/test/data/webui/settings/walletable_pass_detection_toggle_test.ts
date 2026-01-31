// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {EntityDataManagerProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsWalletablePassDetectionToggleElement} from 'chrome://settings/lazy_load.js';

import {TestEntityDataManagerProxy} from './test_entity_data_manager_proxy.js';
// clang-format on

suite('WalletablePassDetectionToggleTest', function() {
  let entityDataManager: TestEntityDataManagerProxy;
  let toggleComponent: SettingsWalletablePassDetectionToggleElement;

  suiteSetup(function() {
    document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    entityDataManager = new TestEntityDataManagerProxy();
    EntityDataManagerProxyImpl.setInstance(entityDataManager);

    loadTimeData.overrideValues(
        {isUserEligibleForWalletablePassDetection: true});

    toggleComponent =
        document.createElement('settings-walletable-pass-detection-toggle');
    document.body.appendChild(toggleComponent);
    await flushTasks();
    entityDataManager.reset();
  });

  test('WalletablePassDetectionToggleUpdatesPref', async function() {
    const toggle = toggleComponent.$.toggle;
    assertTrue(!!toggle);

    // Initial state: not called.
    assertEquals(
        0,
        entityDataManager.getCallCount(
            'setWalletablePassDetectionOptInStatus'));

    // Click to toggle.
    toggle.click();
    await flushTasks();

    // Verify method called with true (since it started false).
    assertTrue(await entityDataManager.whenCalled(
        'setWalletablePassDetectionOptInStatus'));

    // Reset and toggle again.
    entityDataManager.reset();
    toggle.click();
    await flushTasks();

    // Verify method called with false.
    assertFalse(await entityDataManager.whenCalled(
        'setWalletablePassDetectionOptInStatus'));
  });

  test('WalletablePassDetectionToggleUpdatesPrefWithFailure', async function() {
    const toggle = toggleComponent.$.toggle;
    assertTrue(!!toggle);

    // Mock failure response.
    entityDataManager.setSetWalletablePassDetectionOptInStatusResponse(false);

    // Click to toggle ON.
    toggle.click();
    await flushTasks();

    // Verify method called.
    assertTrue(await entityDataManager.whenCalled(
        'setWalletablePassDetectionOptInStatus'));

    // Verify the toggle is unchecked (reverted).
    assertFalse(toggle.checked);
    assertFalse(toggleComponent.get('walletablePassDetectionOptedIn_.value'));
    assertTrue(toggle.disabled);
  });
});
