// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import type {ControlledRadioButtonElement} from 'chrome://settings/settings.js';
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('ControlledRadioButton', () => {
  let radioButton: ControlledRadioButtonElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;

  const initialPrefs = [
    {
      key: 'test_boolean',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    radioButton = document.createElement('controlled-radio-button');
    radioButton.prefKey = 'test_boolean';
    document.body.appendChild(radioButton);
    await microtasksFinished();
  });

  test('disables when pref is managed', async () => {
    assertFalse(radioButton.disabled);

    // Make it managed.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_boolean',
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    }]);
    await microtasksFinished();

    // Verify that policy indicator is shown only when the radio button
    // corresponds to the enforced preference value.
    assertTrue(radioButton.disabled);
    assertFalse(
        !!radioButton.shadowRoot.querySelector('cr-policy-pref-indicator'));

    radioButton.name = 'true';
    await microtasksFinished();
    assertTrue(
        !!radioButton.shadowRoot.querySelector('cr-policy-pref-indicator'));

    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_boolean',
      value: true,
      enforcement: undefined,
    }]);
    await microtasksFinished();
    assertFalse(radioButton.disabled);
    assertFalse(
        !!radioButton.shadowRoot.querySelector('cr-policy-pref-indicator'));
  });

  test('additional content slot is present', async () => {
    const additionalContent = document.createElement('div');
    additionalContent.slot = 'additional-content';
    additionalContent.textContent = 'foo';
    radioButton.appendChild(additionalContent);
    await microtasksFinished();
    const slot = radioButton.shadowRoot.querySelector<HTMLSlotElement>(
        'slot[name="additional-content"]');
    assertTrue(!!slot);
    assertEquals(1, slot.assignedElements().length);
    const slotElement = slot.assignedElements()[0] as HTMLElement;
    assertEquals('foo', slotElement.textContent);
  });
});
