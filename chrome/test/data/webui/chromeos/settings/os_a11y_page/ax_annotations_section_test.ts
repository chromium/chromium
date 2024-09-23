// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://os-settings/os_settings.js';

import type {SettingsAxAnnotationsSectionElement} from 'chrome://os-settings/lazy_load.js';
import {ScreenAiInstallStatus} from 'chrome://os-settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrSettingsPrefs} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite('SettingsAxAnnotationsSectionTest', () => {
  let testElement: SettingsAxAnnotationsSectionElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      mainNodeAnnotationsEnabled: true,
    });
  });

  setup(async () => {
    assertTrue(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));
    clearBody();
    testElement = document.createElement('settings-ax-annotations-section');
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    testElement.prefs = settingsPrefs.prefs;
    document.body.appendChild(testElement);
    flush();
  });

  test('test main node annotations toggle and pref', async () => {
    // Main node annotations toggle visibility depends on the screen reader
    // state, but is managed by a11y_page.ts. Thus, no need to simulate enabling
    // screen reader in this test.
    const toggle =
        testElement.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mainNodeAnnotationsToggle');
    assertTrue(!!toggle);
    await flushTasks();

    // The main node annotations pref is off by default, so the button should be
    // toggled off.
    assertFalse(
        testElement.getPref('settings.a11y.enable_main_node_annotations').value,
        'main node annotations pref should be off by default');
    assertFalse(toggle.checked);

    toggle.click();
    await flushTasks();
    assertTrue(
        testElement.getPref('settings.a11y.enable_main_node_annotations').value,
        'main node annotations pref should be on');
    assertTrue(toggle.checked);
  });

  test('test main node annotations toggle subtitle', async () => {
    // Main node annotations toggle visibility depends on the screen reader
    // state, but is managed by a11y_page.ts. Thus, no need to simulate enabling
    // screen reader in this test.
    const toggle =
        testElement.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mainNodeAnnotationsToggle');
    assertTrue(!!toggle);
    await flushTasks();

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.NOT_DOWNLOADED);
    assertEquals(
        testElement.i18n('mainNodeAnnotationsSubtitle'), toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOAD_FAILED);
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadErrorLabel'),
        toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOADING);
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadingLabel'),
        toggle.subLabel);

    webUIListenerCallback('screen-ai-downloading-progress-changed', 50);
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadProgressLabel', 50),
        toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOADED);
    assertEquals(
        testElement.i18n('mainNodeAnnotationsSubtitle'), toggle.subLabel);
  });
});
