// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DefaultBrowserBrowserProxy, DefaultBrowserInfo, SettingsDefaultBrowserPageElement} from 'chrome://settings/settings.js';
import {DefaultBrowserBrowserProxyImpl} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of DefaultBrowserBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 */
class TestDefaultBrowserBrowserProxy extends TestBrowserProxy implements
    DefaultBrowserBrowserProxy {
  private defaultBrowserInfo_: DefaultBrowserInfo;

  constructor() {
    super([
      'requestDefaultBrowserState',
      'setAsDefaultBrowser',
    ]);

    this.defaultBrowserInfo_ = {
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    };
  }

  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultBrowserInfo_);
  }

  setAsDefaultBrowser() {
    this.methodCalled('setAsDefaultBrowser');
  }

  /**
   * Sets the response to be returned by |requestDefaultBrowserState|.
   * @param info Fake info for testing.
   */
  setDefaultBrowserInfo(info: DefaultBrowserInfo) {
    this.defaultBrowserInfo_ = info;
  }
}

suite('DefaultBrowserPageTest', function() {
  let page: SettingsDefaultBrowserPageElement;
  let browserProxy: TestDefaultBrowserBrowserProxy;

  setup(async function() {
    browserProxy = new TestDefaultBrowserBrowserProxy();
    DefaultBrowserBrowserProxyImpl.setInstance(browserProxy);
    await initPage();
  });

  teardown(function() {
    page.remove();
  });

  async function initPage() {
    browserProxy.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-default-browser-page');
    document.body.appendChild(page);
    await browserProxy.whenCalled('requestDefaultBrowserState');
  }

  test('default-browser-test-can-be-default', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isDefault'));
    assertTrue(
        !page.shadowRoot!.querySelector<HTMLElement>('#isSecondaryInstall'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError'));
  });

  test('default-browser-test-is-default', async function() {
    assertTrue(!!page);
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });

  test('default-browser-test-is-secondary-install', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });

  test('default-browser-test-is-disabled-by-policy', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: true,
      isUnknownError: false,
    });

    await initPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });

  test('default-browser-test-is-unknown-error', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: true,
    });

    await initPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });
});
