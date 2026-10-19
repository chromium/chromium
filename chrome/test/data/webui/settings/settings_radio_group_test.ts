// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import type {SettingsRadioGroupElement} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('SettingsRadioGroup', function() {
  let radioGroup: SettingsRadioGroupElement;
  let proxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

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
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    radioGroup = document.createElement('settings-radio-group');
    document.body.appendChild(radioGroup);
  });

  test('prefKeyProperty', async function() {
    const button1 = document.createElement('controlled-radio-button');
    button1.name = 'option1';
    const button2 = document.createElement('controlled-radio-button');
    button2.name = 'option2';

    radioGroup.appendChild(button1);
    radioGroup.appendChild(button2);
    await microtasksFinished();

    radioGroup.prefKey = 'test.pref';
    await microtasksFinished();

    assertEquals('option1', radioGroup.selected);

    radioGroup.selected = 'option2';
    radioGroup.sendPrefChange();

    const servicePref = prefService.getPref<string>('test.pref');
    assertEquals('option2', servicePref.value);

    proxy.fakeApi.sendPrefChanges([{
      key: 'test.pref',
      value: 'option1',
    }]);
    await microtasksFinished();
    assertEquals('option1', radioGroup.selected);
  });
});
