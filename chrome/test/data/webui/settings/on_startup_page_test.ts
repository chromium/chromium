// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {NtpExtension, OnStartupBrowserProxy, SettingsOnStartupPageElement} from 'chrome://settings/settings.js';
import {OnStartupBrowserProxyImpl, PrefsBrowserProxy, PrefService, PrefValues, StartupUrlsPageBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// <if expr="is_win">
import {loadTimeData} from 'chrome://settings/settings.js';
import type {SettingsToggleButtonElement} from 'chrome://settings/settings.js';

// </if>

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestStartupUrlsPageBrowserProxy} from './test_startup_urls_page_browser_proxy.js';

// clang-format on

class TestOnStartupBrowserProxy extends TestBrowserProxy implements
    OnStartupBrowserProxy {
  private ntpExtension_: NtpExtension|null = null;

  constructor() {
    super(['getNtpExtension']);
  }

  getNtpExtension() {
    this.methodCalled('getNtpExtension');
    return Promise.resolve(this.ntpExtension_);
  }

  /**
   * Sets ntpExtension and fires an update event
   */
  setNtpExtension(ntpExtension: NtpExtension) {
    this.ntpExtension_ = ntpExtension;
    webUIListenerCallback('update-ntp-extension', ntpExtension);
  }
}

function getInitialPrefs(): chrome.settingsPrivate.PrefObject[] {
  return [
    // <if expr="is_win">
    {
      key: 'launch_on_login.foreground.enabled',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    },
    // </if>
    {
      key: 'session.restore_on_startup',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: PrefValues.OPEN_NEW_TAB,
    },
    {
      key: 'session.startup_urls',
      type: chrome.settingsPrivate.PrefType.LIST,
      value: [],
    },
  ];
}

/** @fileoverview Suite of tests for on_startup_page. */
suite('OnStartupPage', function() {
  let testElement: SettingsOnStartupPageElement;
  let onStartupBrowserProxy: TestOnStartupBrowserProxy;
  let startupUrlsBrowserProxy: TestStartupUrlsPageBrowserProxy;
  let prefsBrowserProxy: TestPrefsBrowserProxy;
  let prefService: PrefService;

  const ntpExtension = {id: 'id', name: 'name', canBeDisabled: true};

  const prefValuesToTest: PrefValues[] = [
    PrefValues.CONTINUE,
    PrefValues.OPEN_NEW_TAB,
    PrefValues.OPEN_SPECIFIC,
  ];

  async function initPage(): Promise<void> {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    onStartupBrowserProxy.reset();
    startupUrlsBrowserProxy.reset();
    testElement = document.createElement('settings-on-startup-page');
    document.body.appendChild(testElement);
    await onStartupBrowserProxy.whenCalled('getNtpExtension');
    return microtasksFinished();
  }

  function getSelectedOptionLabel(): string {
    return Array
        .from(
            testElement.shadowRoot.querySelectorAll('controlled-radio-button'))
        .find(
            el => el.name ===
                testElement.shadowRoot.querySelector('settings-radio-group')!
                    .selected)!.label;
  }

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    onStartupBrowserProxy = new TestOnStartupBrowserProxy();
    OnStartupBrowserProxyImpl.setInstance(onStartupBrowserProxy);

    startupUrlsBrowserProxy = new TestStartupUrlsPageBrowserProxy();
    StartupUrlsPageBrowserProxyImpl.setInstance(startupUrlsBrowserProxy);

    prefsBrowserProxy = new TestPrefsBrowserProxy(getInitialPrefs());
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    // <if expr="is_win">
    loadTimeData.overrideValues({isForegroundLaunchFeatureEnabled: false});
    // </if>
    return initPage();
  });

  test('open-continue', async function() {
    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.CONTINUE);
    assertEquals('Continue where you left off', getSelectedOptionLabel());
  });

  test('open-ntp', async function() {
    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.OPEN_NEW_TAB);
    assertEquals('Open the New Tab page', getSelectedOptionLabel());
  });

  test('open-specific', async function() {
    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.OPEN_SPECIFIC);
    assertEquals(
        'Open a specific page or set of pages', getSelectedOptionLabel());
  });

  // Test that loadStartupPages is called every time the "Open a specific page
  // or set of pages" radio button is selected.
  test('toggle-startup-urls-visibility', async function() {
    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.OPEN_SPECIFIC);
    await startupUrlsBrowserProxy.whenCalled('loadStartupPages');
    startupUrlsBrowserProxy.reset();

    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.OPEN_NEW_TAB);
    await prefService.setPrefValue(
        'session.restore_on_startup', PrefValues.OPEN_SPECIFIC);
    await startupUrlsBrowserProxy.whenCalled('loadStartupPages');
  });

  function extensionControlledIndicatorExists() {
    return !!testElement.shadowRoot.querySelector(
        'extension-controlled-indicator');
  }

  test(
      'given ntp extension, extension indicator always exists',
      async function() {
        onStartupBrowserProxy.setNtpExtension(ntpExtension);
        await onStartupBrowserProxy.whenCalled('getNtpExtension');
        assertTrue(extensionControlledIndicatorExists());
        for (const option of prefValuesToTest) {
          await prefService.setPrefValue('session.restore_on_startup', option);
          assertTrue(extensionControlledIndicatorExists());
        }
      });

  test(
      'extension indicator not shown when no ntp extension enabled',
      async function() {
        assertFalse(extensionControlledIndicatorExists());
        for (const option of prefValuesToTest) {
          await prefService.setPrefValue('session.restore_on_startup', option);
          assertFalse(extensionControlledIndicatorExists());
        }
      });

  test('ntp extension updated, extension indicator added', async function() {
    assertFalse(extensionControlledIndicatorExists());
    onStartupBrowserProxy.setNtpExtension(ntpExtension);
    await onStartupBrowserProxy.whenCalled('getNtpExtension');
    assertTrue(extensionControlledIndicatorExists());
  });

  test('searchContents', async function() {
    let result = await testElement.searchContents('Continue where');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await testElement.searchContents('non-existing-text');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await testElement.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
  });

  // <if expr="is_win">
  function getForegroundLaunchToggle(): SettingsToggleButtonElement|null {
    return testElement.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#foregroundLaunchOnStartup');
  }

  [true, false].forEach(featureState => {
    test(
        `settings toggle shown based on feature state: ${featureState}`,
        async function() {
          loadTimeData.overrideValues(
              {isForegroundLaunchFeatureEnabled: featureState});
          await initPage();

          assertEquals(!!getForegroundLaunchToggle(), featureState);
        });
  });

  test('settings toggle state should match pref value', async function() {
    loadTimeData.overrideValues({
      isForegroundLaunchFeatureEnabled: true,
    });
    await initPage();

    const toggleButton = getForegroundLaunchToggle();
    assertTrue(!!toggleButton);

    await prefService.setPrefValue('launch_on_login.foreground.enabled', true);
    assertTrue(toggleButton.checked);

    await prefService.setPrefValue('launch_on_login.foreground.enabled', false);
    assertFalse(toggleButton.checked);
  });
  // </if>
});
