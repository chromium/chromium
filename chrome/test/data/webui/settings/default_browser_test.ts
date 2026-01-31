// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DefaultBrowserBrowserProxy, DefaultBrowserInfo, SettingsDefaultBrowserPageElement} from 'chrome://settings/settings.js';
import {DefaultBrowserBrowserProxyImpl, loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
  private userValueStringsFeatureState_: boolean;

  constructor() {
    super([
      'requestDefaultBrowserState',
      'requestUserValueStringsFeatureState',
      'setAsDefaultBrowser',
    ]);

    this.defaultBrowserInfo_ = {
      canBeDefault: true,
      canPin: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    };

    this.userValueStringsFeatureState_ = false;
  }

  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultBrowserInfo_);
  }

  requestUserValueStringsFeatureState() {
    this.methodCalled('requestUserValueStringsFeatureState');
    return Promise.resolve(this.userValueStringsFeatureState_);
  }

  setAsDefaultBrowser(pin: boolean) {
    this.methodCalled('setAsDefaultBrowser', pin);
  }

  /**
   * Sets the response to be returned by |requestDefaultBrowserState|.
   * @param info Fake info for testing.
   */
  setDefaultBrowserInfo(info: DefaultBrowserInfo) {
    this.defaultBrowserInfo_ = info;
  }

  /**
   * Sets the response to be returned by |requestUserValueStringsFeatureState|.
   * @param featureState Fake state for testing.
   */
  setUserValueStringsFeatureState(featureState: boolean) {
    this.userValueStringsFeatureState_ = featureState;
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

  /**
   * Simulates navigation to the Default Browser page and waits
   * for the experiment activation call.
   */
  function navigateToDefaultBrowserPage() {
    browserProxy.resetResolver('requestUserValueStringsFeatureState');
    Router.getInstance().navigateTo(routes.DEFAULT_BROWSER);
    return browserProxy.whenCalled('requestUserValueStringsFeatureState');
  }

  test('default-browser-test-can-be-default-featureOff', async function() {
    browserProxy.setUserValueStringsFeatureState(false);

    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      canPin: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    await navigateToDefaultBrowserPage();
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isDefault'));
    assertTrue(
        !page.shadowRoot!.querySelector<HTMLElement>('#isSecondaryInstall'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError'));
    // Verify that settings page doesn't offer to pin Chrome.
    const makeDefault =
        page.shadowRoot!.querySelector<HTMLElement>('#makeDefaultLabel');
    assertTrue(!!makeDefault);
    assertEquals(
        makeDefault.textContent.trim(),
        loadTimeData.getString('defaultBrowserMakeDefault'));
  });

  test('default-browser-test-can-be-default-featureOn', async function() {
    browserProxy.setUserValueStringsFeatureState(true);

    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      canPin: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    await navigateToDefaultBrowserPage();  // Triggers currentRouteChanged
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isDefault'));
    assertTrue(
        !page.shadowRoot!.querySelector<HTMLElement>('#isSecondaryInstall'));
    assertTrue(!page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError'));
    // Verify that settings page doesn't offer to pin Chrome.
    const makeDefault =
        page.shadowRoot!.querySelector<HTMLElement>('#makeDefaultLabel');
    assertTrue(!!makeDefault);
    assertEquals(
        makeDefault.textContent.trim(),
        loadTimeData.getString('defaultBrowserMakeDefaultUserValue'));
  });

  test('default-browser-test-can-be-default-and-pin', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      canPin: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    flush();
    assertTrue(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertFalse(!!page.shadowRoot!.querySelector<HTMLElement>('#isDefault'));
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#isSecondaryInstall'));
    // Verify that settings page offers to pin Chrome.
    const makeDefault =
        page.shadowRoot!.querySelector<HTMLElement>('#makeDefaultLabel');
    assertTrue(!!makeDefault);
    assertEquals(
        makeDefault.textContent.trim(),
        loadTimeData.getString('defaultBrowserMakeDefaultAndPin'));
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError'));
  });

  test('default-browser-test-is-default-featureOff', async function() {
    assertTrue(!!page);

    browserProxy.setUserValueStringsFeatureState(false);

    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      canPin: false,
      isDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    await navigateToDefaultBrowserPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#defaultStringThankYou')!.hidden);
    const defaultString =
        page.shadowRoot!.querySelector<HTMLElement>('#defaultString');
    assertFalse(defaultString!.hidden);
    assertEquals(
        defaultString!.textContent.trim(),
        loadTimeData.getString('defaultBrowserDefault'));

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });

  test('default-browser-test-is-default-featureOn', async function() {
    assertTrue(!!page);

    browserProxy.setUserValueStringsFeatureState(true);

    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      canPin: false,
      isDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false,
    });

    await initPage();
    await navigateToDefaultBrowserPage();
    flush();
    assertFalse(
        !!page.shadowRoot!.querySelector<HTMLElement>('#canBeDefaultBrowser'));
    assertFalse(
        page.shadowRoot!.querySelector<HTMLElement>('#isDefault')!.hidden);

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#defaultString')!.hidden);
    const defaultStringThankYou =
        page.shadowRoot!.querySelector<HTMLElement>('#defaultStringThankYou');
    assertFalse(defaultStringThankYou!.hidden);
    assertEquals(
        defaultStringThankYou!.textContent.trim(),
        loadTimeData.getString('defaultBrowserDefaultThankYou'));

    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>(
                            '#isSecondaryInstall')!.hidden);
    assertTrue(
        page.shadowRoot!.querySelector<HTMLElement>('#isUnknownError')!.hidden);
  });

  test('default-browser-test-is-secondary-install', async function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: false,
      canPin: false,
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
      canPin: false,
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
      canPin: false,
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

  test('searchContents', async function() {
    let result = await page.searchContents('Make default');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await page.searchContents('non-existing-text');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await page.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
  });
});
