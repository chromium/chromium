// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {ControlledButtonElement} from 'chrome://settings/lazy_load.js';
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('controlled button', function() {
  let controlledButton: ControlledButtonElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;

  const initialPrefs = [
    {
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    controlledButton = document.createElement('controlled-button');
    controlledButton.prefKey = 'test';
    document.body.appendChild(controlledButton);
    await microtasksFinished();
  });

  test('controlled prefs', async function() {
    assertFalse(controlledButton.$.button.disabled);
    assertFalse(!!controlledButton.shadowRoot.querySelector(
        'cr-policy-pref-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      value: true,
      controlledBy: chrome.settingsPrivate.ControlledBy.EXTENSION,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    await microtasksFinished();
    assertTrue(controlledButton.$.button.disabled);
    assertTrue(!!controlledButton.shadowRoot.querySelector(
        'cr-policy-pref-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      value: true,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    await microtasksFinished();
    assertTrue(controlledButton.$.button.disabled);
    const indicator =
        controlledButton.shadowRoot.querySelector('cr-policy-pref-indicator');
    assertTrue(!!indicator);
    assertGT(indicator.clientHeight, 0);

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test',
      value: true,
      controlledBy: undefined,
      enforcement: undefined,
    }]);
    await microtasksFinished();
    assertFalse(controlledButton.$.button.disabled);
    assertFalse(!!controlledButton.shadowRoot.querySelector(
        'cr-policy-pref-indicator'));
  });

  test('action-button', async function() {
    assertNotEquals('action-button', controlledButton.$.button.className);

    const controlledActionButton = document.createElement('controlled-button');
    controlledActionButton.prefKey = 'test';
    controlledActionButton.className = 'action-button';
    document.body.appendChild(controlledActionButton);
    await microtasksFinished();
    assertEquals('action-button', controlledActionButton.$.button.className);
  });
});
