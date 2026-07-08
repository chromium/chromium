// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import type {SettingsRadioGroupElement} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('SettingsRadioGroup', function() {
  let radioGroup: SettingsRadioGroupElement;
  let proxy: TestPrefsBrowserProxy;

  const initialPrefs = [
    {
      key: 'test.pref',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'option1',
    },
  ];

  setup(async function() {
    proxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(proxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioGroup = document.createElement('settings-radio-group');
    document.body.appendChild(radioGroup);
  });

  // Test the old 'pref' mechanism.
  test('prefProperty', function() {
    const prefCopy = structuredClone(initialPrefs[0]);
    radioGroup.pref = prefCopy;
    flush();

    const button1 = document.createElement('controlled-radio-button');
    button1.name = 'option1';
    button1.pref = prefCopy;
    const button2 = document.createElement('controlled-radio-button');
    button2.name = 'option2';
    button2.pref = prefCopy;

    radioGroup.appendChild(button1);
    radioGroup.appendChild(button2);
    flush();

    assertEquals('option1', radioGroup.selected);

    radioGroup.selected = 'option2';
    radioGroup.sendPrefChange();
    assertEquals('option2', radioGroup.pref!.value);
  });

  // Test the new 'prefKey' mechanism.
  test('prefKeyProperty', async function() {
    const button1 = document.createElement('controlled-radio-button');
    button1.name = 'option1';
    const button2 = document.createElement('controlled-radio-button');
    button2.name = 'option2';

    radioGroup.appendChild(button1);
    radioGroup.appendChild(button2);
    flush();

    radioGroup.prefKey = 'test.pref';
    await microtasksFinished();

    assertTrue(!!radioGroup.pref);
    assertEquals('test.pref', radioGroup.pref.key);
    assertEquals('option1', radioGroup.selected);

    radioGroup.selected = 'option2';
    radioGroup.sendPrefChange();

    const servicePref = PrefService.getInstance().getPref<string>('test.pref');
    assertEquals('option2', servicePref.value);

    await microtasksFinished();
    assertEquals('option2', radioGroup.pref.value);
  });
});
