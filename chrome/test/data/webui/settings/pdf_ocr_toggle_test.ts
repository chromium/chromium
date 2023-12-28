// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ScreenAiInstallStatus, SettingsPdfOcrToggleElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('SettingsPdfOcrToggleTest', () => {
  let testElement: SettingsPdfOcrToggleElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      pdfOcrEnabled: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-pdf-ocr-toggle');
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    testElement.prefs = settingsPrefs.prefs;
    document.body.appendChild(testElement);
    flush();
  });

  test('test pdf ocr toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));
    // Simulate enabling a screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    const toggle = testElement.$.toggle;
    await flushTasks();

    // The PDF OCR pref is on by default, so the button should be toggled on.
    assertTrue(
        testElement.getPref('settings.a11y.pdf_ocr_always_active').value,
        'pdf ocr pref should be on by default');
    assertTrue(toggle.checked);

    toggle.click();
    await flushTasks();
    assertFalse(
        testElement.getPref('settings.a11y.pdf_ocr_always_active').value,
        'pdf ocr pref should be off');
    assertFalse(toggle.checked);
  });

  test('test pdf ocr toggle subtitle', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));
    // Simulate enabling a screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    const toggle = testElement.$.toggle;
    await flushTasks();

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.NOT_DOWNLOADED);
    assertEquals(testElement.i18n('pdfOcrSubtitle'), toggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.FAILED);
    assertEquals(testElement.i18n('pdfOcrDownloadErrorLabel'), toggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADING);
    assertEquals(testElement.i18n('pdfOcrDownloadingLabel'), toggle.subLabel);

    webUIListenerCallback('pdf-ocr-downloading-progress-changed', 50);
    assertEquals(
        testElement.i18n('pdfOcrDownloadProgressLabel', 50), toggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADED);
    assertEquals(
        testElement.i18n('pdfOcrDownloadCompleteLabel'), toggle.subLabel);

    webUIListenerCallback('pdf-ocr-state-changed', ScreenAiInstallStatus.READY);
    assertEquals(testElement.i18n('pdfOcrSubtitle'), toggle.subLabel);
  });
});
