// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {NtpExtension, OnStartupBrowserProxy, SettingsOnStartupPageElement} from 'chrome://settings/settings.js';
import {OnStartupBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
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

/** @fileoverview Suite of tests for on_startup_page. */
suite('OnStartupPage', function() {
  /**
   * Radio button enum values for restore on startup.
   * @enum
   */
  const RestoreOnStartupEnum = {
    CONTINUE: 1,
    OPEN_NEW_TAB: 5,
    OPEN_SPECIFIC: 4,
  };

  let testElement: SettingsOnStartupPageElement;
  let onStartupBrowserProxy: TestOnStartupBrowserProxy;

  const ntpExtension = {id: 'id', name: 'name', canBeDisabled: true};

  async function initPage(): Promise<void> {
    onStartupBrowserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('settings-on-startup-page');
    testElement.prefs = {
      session: {
        restore_on_startup: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: RestoreOnStartupEnum.OPEN_NEW_TAB,
        },
      },
    };
    document.body.appendChild(testElement);
    await onStartupBrowserProxy.whenCalled('getNtpExtension');
    flush();
  }

  function getSelectedOptionLabel(): string {
    return Array
        .from(
            testElement.shadowRoot!.querySelectorAll('controlled-radio-button'))
        .find(
            el => el.name ===
                testElement.shadowRoot!.querySelector('settings-radio-group')!
                    .selected)!.label;
  }

  setup(async function() {
    onStartupBrowserProxy = new TestOnStartupBrowserProxy();
    OnStartupBrowserProxyImpl.setInstance(onStartupBrowserProxy);
    await initPage();
  });

  teardown(function() {
    if (testElement) {
      testElement.remove();
    }
  });

  test('open-continue', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.CONTINUE);
    assertEquals('Continue where you left off', getSelectedOptionLabel());
  });

  test('open-ntp', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.OPEN_NEW_TAB);
    assertEquals('Open the New Tab page', getSelectedOptionLabel());
  });

  test('open-specific', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.OPEN_SPECIFIC);
    assertEquals(
        'Open a specific page or set of pages', getSelectedOptionLabel());
  });

  function extensionControlledIndicatorExists() {
    return !!testElement.shadowRoot!.querySelector(
        'extension-controlled-indicator');
  }

  test(
      'given ntp extension, extension indicator always exists',
      async function() {
        onStartupBrowserProxy.setNtpExtension(ntpExtension);
        await onStartupBrowserProxy.whenCalled('getNtpExtension');
        flush();
        assertTrue(extensionControlledIndicatorExists());
        Object.values(RestoreOnStartupEnum).forEach(function(option) {
          testElement.set('prefs.session.restore_on_startup.value', option);
          assertTrue(extensionControlledIndicatorExists());
        });
      });

  test(
      'extension indicator not shown when no ntp extension enabled',
      function() {
        assertFalse(extensionControlledIndicatorExists());
        Object.values(RestoreOnStartupEnum).forEach(function(option) {
          testElement.set('prefs.session.restore_on_startup.value', option);
          assertFalse(extensionControlledIndicatorExists());
        });
      });

  test('ntp extension updated, extension indicator added', async function() {
    assertFalse(extensionControlledIndicatorExists());
    onStartupBrowserProxy.setNtpExtension(ntpExtension);
    await onStartupBrowserProxy.whenCalled('getNtpExtension');
    flush();
    assertTrue(extensionControlledIndicatorExists());
  });
});
