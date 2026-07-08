// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ControlledRadioButtonElement} from 'chrome://settings/settings.js';
import {PrefService, PrefsBrowserProxy} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
// clang-format on

suite('controlled radio button', function() {
  let radioButton: ControlledRadioButtonElement;

  const pref: chrome.settingsPrivate.PrefObject = {
    key: 'test',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
  };

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioButton = document.createElement('controlled-radio-button');
    radioButton.set('pref', pref);
    document.body.appendChild(radioButton);
  });

  test('disables when pref is managed', function() {
    radioButton.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    flush();
    assertTrue(radioButton.disabled);
    assertFalse(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    radioButton.set('name', 'true');
    flush();
    assertTrue(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    // See https://github.com/Polymer/polymer/issues/4652#issuecomment-305471987
    // on why |null| must be used here instead of |undefined|.
    radioButton.set('pref.enforcement', null);
    flush();
    assertFalse(radioButton.disabled);
    assertEquals(
        'none',
        radioButton.shadowRoot!.querySelector(
                                   'cr-policy-pref-indicator')!.style.display);
  });

  test('additional content slot is present', function() {
    const additionalContent = document.createElement('div');
    additionalContent.slot = 'additional-content';
    additionalContent.textContent = 'foo';
    radioButton.appendChild(additionalContent);
    flush();
    const slot = radioButton.shadowRoot!.querySelector<HTMLSlotElement>(
        'slot[name="additional-content"]');
    assertTrue(!!slot);
    assertEquals(1, slot.assignedElements().length);
    const slotElement = slot.assignedElements()[0] as HTMLElement;
    assertEquals('foo', slotElement.textContent);
  });
});

suite('ControlledRadioButtonPrefKey', () => {
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
    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);

    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioButton = document.createElement('controlled-radio-button');
    radioButton.prefKey = 'test_boolean';
    document.body.appendChild(radioButton);
  });

  test('disablesWhenPrefIsManaged', async () => {
    assertFalse(radioButton.disabled);

    // Make it managed.
    const pref = prefsBrowserProxy.fakeApi.prefs['test_boolean']!;
    pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    prefsBrowserProxy.fakeApi.sendPrefChanges(
        [{key: 'test_boolean', value: true}]);
    await microtasksFinished();

    // Verify that policy indicator is shown only when the radio button
    // corresponds to the enforced preference value.
    assertTrue(radioButton.disabled);
    assertFalse(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));

    radioButton.name = 'true';
    await microtasksFinished();
    assertTrue(
        !!radioButton.shadowRoot!.querySelector('cr-policy-pref-indicator'));
  });
});
