// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsFaceGazeSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<settings-facegaze-subpage>', () => {
  let faceGazeSubpage: SettingsFaceGazeSubpageElement;
  let prefElement: SettingsPrefsElement;

  function getToggleButton(): SettingsToggleButtonElement|null {
    return faceGazeSubpage.shadowRoot!
        .querySelector<SettingsToggleButtonElement>('#faceGazeToggle');
  }

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    faceGazeSubpage = document.createElement('settings-facegaze-subpage');
    faceGazeSubpage.prefs = prefElement.prefs;
    document.body.appendChild(faceGazeSubpage);
    flush();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_FACEGAZE_SETTINGS);
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('toggle button reflects pref value', async () => {
    await initPage();
    faceGazeSubpage.set('prefs.settings.a11y.face_gaze.enabled.value', true);
    await flushTasks();

    assertTrue(faceGazeSubpage.prefs.settings.a11y.face_gaze.enabled.value);

    const toggle = getToggleButton();
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    assertTrue(toggle.checked);
    assertEquals('On', toggle.label);
  });

  test('clicking toggle button updates pref value', async () => {
    await initPage();

    assertFalse(faceGazeSubpage.prefs.settings.a11y.face_gaze.enabled.value);

    const toggle = getToggleButton();
    assertTrue(!!toggle);
    assertTrue(isVisible(toggle));
    assertFalse(toggle.checked);
    assertEquals('Off', toggle.label);

    toggle.click();
    await flushTasks();

    assertTrue(toggle.checked);
    assertTrue(faceGazeSubpage.prefs.settings.a11y.face_gaze.enabled.value);
    assertEquals('On', toggle.label);
  });
});
