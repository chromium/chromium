// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {AxAnnotationsBrowserProxy, SettingsAxAnnotationsSectionElement, SettingsToggleButtonElement} from 'chrome://settings/lazy_load.js';
import {AxAnnotationsBrowserProxyImpl, ScreenAiInstallStatus} from 'chrome://settings/lazy_load.js';
import {loadTimeData, PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

class TestAxAnnotationsBrowserProxy extends TestBrowserProxy implements
    AxAnnotationsBrowserProxy {
  private screenAiInstallStatus_: ScreenAiInstallStatus =
      ScreenAiInstallStatus.NOT_DOWNLOADED;

  constructor() {
    super([
      'getScreenAiInstallState',
    ]);
  }

  setScreenAiInstallStatus(status: ScreenAiInstallStatus) {
    this.screenAiInstallStatus_ = status;
  }

  getScreenAiInstallState() {
    this.methodCalled('getScreenAiInstallState');
    return Promise.resolve(this.screenAiInstallStatus_);
  }
}

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    {
      key: 'settings.a11y.enable_main_node_annotations',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
  ];
}

suite('SettingsAxAnnotationsSectionTest', () => {
  let testElement: SettingsAxAnnotationsSectionElement;
  let prefService: PrefService;
  let browserProxy: TestAxAnnotationsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      mainNodeAnnotationsEnabled: true,
    });
  });

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestAxAnnotationsBrowserProxy();
    browserProxy.setScreenAiInstallStatus(ScreenAiInstallStatus.NOT_DOWNLOADED);
    AxAnnotationsBrowserProxyImpl.setInstanceForTesting(browserProxy);

    const prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    testElement = document.createElement('settings-ax-annotations-section');
    document.body.appendChild(testElement);
    await browserProxy.whenCalled('getScreenAiInstallState');
    return microtasksFinished();
  });

  test('main node annotations toggle and pref', async () => {
    assertTrue(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    // Main node annotations toggle visibility depends on the screen reader
    // state, but is managed by a11y_page.ts. Thus, no need to simulate enabling
    // screen reader in this test.
    const toggle =
        testElement.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#mainNodeAnnotationsToggle');
    assertTrue(!!toggle);

    // The main node annotations pref is off by default, so the button should be
    // toggled off.
    assertFalse(
        prefService
            .getPref<boolean>('settings.a11y.enable_main_node_annotations')
            .value,
        'main node annotations pref should be off by default');
    assertFalse(toggle.checked);

    toggle.click();
    await microtasksFinished();
    assertTrue(
        prefService
            .getPref<boolean>('settings.a11y.enable_main_node_annotations')
            .value,
        'main node annotations pref should be on');
    assertTrue(toggle.checked);
  });

  test('main node annotations toggle subtitle', async () => {
    assertTrue(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    // Main node annotations toggle visibility depends on the screen reader
    // state, but is managed by a11y_page.ts. Thus, no need to simulate enabling
    // screen reader in this test.
    const toggle =
        testElement.shadowRoot.querySelector<SettingsToggleButtonElement>(
            '#mainNodeAnnotationsToggle');
    assertTrue(!!toggle);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.NOT_DOWNLOADED);
    await microtasksFinished();
    assertEquals(
        testElement.i18n('mainNodeAnnotationsSubtitle'), toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOAD_FAILED);
    await microtasksFinished();
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadErrorLabel'),
        toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOADING);
    await microtasksFinished();
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadingLabel'),
        toggle.subLabel);

    webUIListenerCallback('screen-ai-downloading-progress-changed', 50);
    await microtasksFinished();
    assertEquals(
        testElement.i18n('mainNodeAnnotationsDownloadProgressLabel', 50),
        toggle.subLabel);

    webUIListenerCallback(
        'screen-ai-state-changed', ScreenAiInstallStatus.DOWNLOADED);
    await microtasksFinished();
    assertEquals(
        testElement.i18n('mainNodeAnnotationsSubtitle'), toggle.subLabel);
  });
});
