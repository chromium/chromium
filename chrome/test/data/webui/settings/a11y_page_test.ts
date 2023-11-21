// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// clang-format off
// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ScreenAiInstallStatus} from 'chrome://settings/lazy_load.js';
import {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// </if>
// clang-format on

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AccessibilityBrowserProxy, AccessibilityBrowserProxyImpl, SettingsA11yPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestAccessibilityBrowserProxy extends TestBrowserProxy implements
    AccessibilityBrowserProxy {
  constructor() {
    super([
      'openTrackpadGesturesSettings',
      'recordOverscrollHistoryNavigationChanged',
    ]);
  }

  openTrackpadGesturesSettings() {
    this.methodCalled('openTrackpadGesturesSettings');
  }

  recordOverscrollHistoryNavigationChanged(enabled: boolean) {
    this.methodCalled('recordOverscrollHistoryNavigationChanged', enabled);
  }
}

suite('A11yPage', () => {
  let a11yPage: SettingsA11yPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let browserProxy: TestAccessibilityBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      pdfOcrEnabled: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    a11yPage = document.createElement('settings-a11y-page');
    settingsPrefs = document.createElement('settings-prefs');
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    a11yPage.prefs = settingsPrefs.prefs;
    document.body.appendChild(a11yPage);

    // Set up test browser proxy.
    browserProxy = new TestAccessibilityBrowserProxy();
    AccessibilityBrowserProxyImpl.setInstance(browserProxy);

    flush();
  });

  // <if expr="is_win or is_linux or is_macosx">
  test('check pdf ocr toggle visibility', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));

    // Simulate disabling a screen reader to hide the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', false);

    const pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    await flushTasks();
    assertFalse(isVisible(pdfOcrToggle));

    // Simulate enabling a screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    await flushTasks();
    assertTrue(isVisible(pdfOcrToggle));
  });

  test('test pdf ocr toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));
    // Simulate enabling a screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    const pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    await flushTasks();

    // The PDF OCR pref is on by default, so the button should be toggled on.
    assertTrue(
        a11yPage.getPref('settings.a11y.pdf_ocr_always_active').value,
        'pdf ocr pref should be on by default');
    assertTrue(pdfOcrToggle.checked);

    pdfOcrToggle.click();
    await flushTasks();
    assertFalse(
        a11yPage.getPref('settings.a11y.pdf_ocr_always_active').value,
        'pdf ocr pref should be off');
    assertFalse(pdfOcrToggle.checked);
  });

  test('test pdf ocr toggle subtitle', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));
    // Simulate enabling a screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    const pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    await flushTasks();

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.NOT_DOWNLOADED);
    assertEquals(a11yPage.i18n('pdfOcrSubtitle'), pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.FAILED);
    assertEquals(
        a11yPage.i18n('pdfOcrDownloadErrorLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADING);
    assertEquals(
        a11yPage.i18n('pdfOcrDownloadingLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback('pdf-ocr-downloading-progress-changed', 50);
    assertEquals(
        a11yPage.i18n('pdfOcrDownloadProgressLabel', 50),
        pdfOcrToggle.subLabel);

    webUIListenerCallback(
        'pdf-ocr-state-changed', ScreenAiInstallStatus.DOWNLOADED);
    assertEquals(
        a11yPage.i18n('pdfOcrDownloadCompleteLabel'), pdfOcrToggle.subLabel);

    webUIListenerCallback('pdf-ocr-state-changed', ScreenAiInstallStatus.READY);
    assertEquals(a11yPage.i18n('pdfOcrSubtitle'), pdfOcrToggle.subLabel);
  });
  // </if>

  // TODO(crbug.com/1499996): Add more test cases to improve code coverage.
});
