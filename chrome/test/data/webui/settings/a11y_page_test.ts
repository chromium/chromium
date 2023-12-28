// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

// clang-format off
// <if expr="is_win or is_linux or is_macosx">
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {ScreenAiInstallStatus, SettingsPdfOcrToggleElement} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
  private pdfOcrState_: ScreenAiInstallStatus;

  constructor() {
    super([
      'openTrackpadGesturesSettings',
      'recordOverscrollHistoryNavigationChanged',
      // <if expr="is_win or is_linux or is_macosx">
      'getScreenAiInstallState',
      // </if>
      'getScreenReaderState',
    ]);

    this.pdfOcrState_ = ScreenAiInstallStatus.NOT_DOWNLOADED;
  }

  openTrackpadGesturesSettings() {
    this.methodCalled('openTrackpadGesturesSettings');
  }

  recordOverscrollHistoryNavigationChanged(enabled: boolean) {
    this.methodCalled('recordOverscrollHistoryNavigationChanged', enabled);
  }

  // <if expr="is_win or is_linux or is_macosx">
  getScreenAiInstallState() {
    this.methodCalled('getScreenAiInstallState');
    return Promise.resolve(this.pdfOcrState_);
  }
  // </if>

  getScreenReaderState() {
    this.methodCalled('getScreenReaderState');
    return Promise.resolve(false);
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

    // Simulate disabling a screen reader to exclude the PDF OCR toggle in a
    // DOM.
    webUIListenerCallback('screen-reader-state-changed', false);

    await flushTasks();
    let pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsPdfOcrToggleElement>(
            '#pdfOcrToggle');
    assertFalse(!!pdfOcrToggle);

    // Simulate enabling a screen reader to include the PDF OCR toggle in a
    // DOM.
    webUIListenerCallback('screen-reader-state-changed', true);

    await flushTasks();
    pdfOcrToggle =
        a11yPage.shadowRoot!.querySelector<SettingsPdfOcrToggleElement>(
            '#pdfOcrToggle');
    assertTrue(!!pdfOcrToggle);
    assertTrue(isVisible(pdfOcrToggle));
  });
  // </if>

  // TODO(crbug.com/1499996): Add more test cases to improve code coverage.
});
