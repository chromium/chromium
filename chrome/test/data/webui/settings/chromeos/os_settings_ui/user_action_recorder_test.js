// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes, setNearbyShareSettingsForTesting, setUserActionRecorderForTesting} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {FakeNearbyShareSettings} from 'chrome://webui-test/nearby_share/shared/fake_nearby_share_settings.js';

import {FakeUserActionRecorder} from '../fake_user_action_recorder.js';

suite('User action recorder', () => {
  let ui;
  let fakeUserActionRecorder;
  let fakeNearbySettings;

  async function createElement() {
    const element = document.createElement('os-settings-ui');
    document.body.appendChild(element);
    await CrSettingsPrefs.initialized;
    flush();
    return element;
  }

  suiteSetup(async () => {
    fakeNearbySettings = new FakeNearbyShareSettings();
    setNearbyShareSettingsForTesting(fakeNearbySettings);
  });

  setup(async () => {
    fakeUserActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(fakeUserActionRecorder);
    ui = await createElement();
  });

  teardown(() => {
    ui.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Records navigation changes', () => {
    assertEquals(0, fakeUserActionRecorder.navigationCount);

    Router.getInstance().navigateTo(routes.INTERNET);
    assertEquals(1, fakeUserActionRecorder.navigationCount);

    Router.getInstance().navigateTo(routes.BASIC);
    assertEquals(2, fakeUserActionRecorder.navigationCount);
  });

  test('Records blur events', () => {
    assertEquals(0, fakeUserActionRecorder.pageBlurCount);
    window.dispatchEvent(new Event('blur'));
    assertEquals(1, fakeUserActionRecorder.pageBlurCount);
  });

  test('Records click events', () => {
    assertEquals(0, fakeUserActionRecorder.clickCount);
    ui.click();
    assertEquals(1, fakeUserActionRecorder.clickCount);
  });

  test('Records focus events', () => {
    // Focus is already recorded when the page is loaded
    assertEquals(1, fakeUserActionRecorder.pageFocusCount);
    window.dispatchEvent(new Event('focus'));
    assertEquals(2, fakeUserActionRecorder.pageFocusCount);
  });

  test('Records settings changes', () => {
    assertEquals(0, fakeUserActionRecorder.settingChangeCount);
    const prefsEl = ui.shadowRoot.querySelector('#prefs');
    prefsEl.dispatchEvent(new CustomEvent('user-action-setting-change', {
      bubbles: true,
      composed: true,
      detail: {prefKey: 'foo', prefValue: 'bar'},
    }));
    assertEquals(1, fakeUserActionRecorder.settingChangeCount);
  });
});
