// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {DefaultBrowserBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
// clang-format on

/**
 * A test version of DefaultBrowserBrowserProxy. Provides helper methods
 * for allowing tests to know when a method was called, as well as
 * specifying mock responses.
 *
 * @implements {DefaultBrowserBrowserProxy}
 */
class TestDefaultBrowserBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestDefaultBrowserState',
      'setAsDefaultBrowser',
    ]);

    /** @private {!DefaultBrowserInfo} */
    this.defaultBrowserInfo_ = {
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    };
  }

  /** @override */
  requestDefaultBrowserState() {
    this.methodCalled('requestDefaultBrowserState');
    return Promise.resolve(this.defaultBrowserInfo_);
  }

  /** @override */
  setAsDefaultBrowser() {
    this.methodCalled('setAsDefaultBrowser');
  }

  /**
   * Sets the response to be returned by |requestDefaultBrowserState|.
   * @param {!DefaultBrowserInfo} info Fake info for testing.
   */
  setDefaultBrowserInfo(info) {
    this.defaultBrowserInfo_ = info;
  }
}

suite('DefaultBrowserPageTest', function() {
  let page = null;

  /** @type {?settings.TestDefaultBrowserBrowserProxy} */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestDefaultBrowserBrowserProxy();
    DefaultBrowserBrowserProxyImpl.setInstance(browserProxy);
    return initPage();
  });

  teardown(function() {
    page.remove();
    page = null;
  });

  /** @return {!Promise} */
  function initPage() {
    browserProxy.reset();
    PolymerTest.clearBody();
    page = document.createElement('settings-default-browser-page');
    document.body.appendChild(page);
    return browserProxy.whenCalled('requestDefaultBrowserState');
  }

  test('default-browser-test-can-be-default', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertTrue(!!page.shadowRoot.querySelector('#canBeDefaultBrowser'));
      assertTrue(!page.shadowRoot.querySelector('#isDefault'));
      assertTrue(!page.shadowRoot.querySelector('#isSecondaryInstall'));
      assertTrue(!page.shadowRoot.querySelector('#isUnknownError'));
    });
  });

  test('default-browser-test-is-default', function() {
    assertTrue(!!page);
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: true,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot.querySelector('#canBeDefaultBrowser'));
      assertFalse(page.shadowRoot.querySelector('#isDefault').hidden);
      assertTrue(page.shadowRoot.querySelector('#isSecondaryInstall').hidden);
      assertTrue(page.shadowRoot.querySelector('#isUnknownError').hidden);
    });
  });

  test('default-browser-test-is-secondary-install', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: false,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot.querySelector('#canBeDefaultBrowser'));
      assertTrue(page.shadowRoot.querySelector('#isDefault').hidden);
      assertFalse(page.shadowRoot.querySelector('#isSecondaryInstall').hidden);
      assertTrue(page.shadowRoot.querySelector('#isUnknownError').hidden);
    });
  });

  test('default-browser-test-is-disabled-by-policy', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: true,
      isUnknownError: false
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot.querySelector('#canBeDefaultBrowser'));
      assertTrue(page.shadowRoot.querySelector('#isDefault').hidden);
      assertTrue(page.shadowRoot.querySelector('#isSecondaryInstall').hidden);
      assertFalse(page.shadowRoot.querySelector('#isUnknownError').hidden);
    });
  });

  test('default-browser-test-is-unknown-error', function() {
    browserProxy.setDefaultBrowserInfo({
      canBeDefault: true,
      isDefault: false,
      isDisabledByPolicy: false,
      isUnknownError: true
    });

    return initPage().then(function() {
      flush();
      assertFalse(!!page.shadowRoot.querySelector('#canBeDefaultBrowser'));
      assertTrue(page.shadowRoot.querySelector('#isDefault').hidden);
      assertTrue(page.shadowRoot.querySelector('#isSecondaryInstall').hidden);
      assertFalse(page.shadowRoot.querySelector('#isUnknownError').hidden);
    });
  });
});
