// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
// </if>
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AccessibilityBrowserProxy, AccessibilityBrowserProxyImpl, SettingsA11yPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, SettingsPrefsElement} from 'chrome://settings/settings.js';
// <if expr="is_win or is_linux or is_macosx">
import {SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// </if>
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// <if expr="is_win or is_linux or is_macosx">
import {isVisible} from 'chrome://webui-test/test_util.js';
// </if>

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

    // Disable a screen reader to hide the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', false);

    const pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    await flushTasks();
    assertFalse(isVisible(pdfOcrToggle));

    // Enable screen reader to show the PDF OCR toggle.
    webUIListenerCallback('screen-reader-state-changed', true);

    await flushTasks();
    assertTrue(isVisible(pdfOcrToggle));
  });

  test('test pdf ocr toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('pdfOcrEnabled'));
    // Enable screen reader to show the PDF OCR toggle.
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
    flush();
    assertFalse(
        a11yPage.getPref('settings.a11y.pdf_ocr_always_active').value,
        'pdf ocr pref should be off');
    assertFalse(pdfOcrToggle.checked);
  });
  // </if>

  // TODO(crbug.com/1499996): Add more test cases to improve code coverage.
});
