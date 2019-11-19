// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_default_browser', function() {
  /**
   * A test version of DefaultBrowserBrowserProxy. Provides helper methods
   * for allowing tests to know when a method was called, as well as
   * specifying mock responses.
   *
   * @implements {settings.DefaultBrowserBrowserProxy}
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
      settings.DefaultBrowserBrowserProxyImpl.instance_ = browserProxy;
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
        Polymer.dom.flush();
        assertTrue(!!page.$$('#canBeDefaultBrowser'));
        assertTrue(!page.$$('#isDefault'));
        assertTrue(!page.$$('#isSecondaryInstall'));
        assertTrue(!page.$$('#isUnknownError'));
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
        Polymer.dom.flush();
        assertFalse(!!page.$$('#canBeDefaultBrowser'));
        assertFalse(page.$$('#isDefault').hidden);
        assertTrue(page.$$('#isSecondaryInstall').hidden);
        assertTrue(page.$$('#isUnknownError').hidden);
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
        Polymer.dom.flush();
        assertFalse(!!page.$$('#canBeDefaultBrowser'));
        assertTrue(page.$$('#isDefault').hidden);
        assertFalse(page.$$('#isSecondaryInstall').hidden);
        assertTrue(page.$$('#isUnknownError').hidden);
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
        Polymer.dom.flush();
        assertFalse(!!page.$$('#canBeDefaultBrowser'));
        assertTrue(page.$$('#isDefault').hidden);
        assertTrue(page.$$('#isSecondaryInstall').hidden);
        assertFalse(page.$$('#isUnknownError').hidden);
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
        Polymer.dom.flush();
        assertFalse(!!page.$$('#canBeDefaultBrowser'));
        assertTrue(page.$$('#isDefault').hidden);
        assertTrue(page.$$('#isSecondaryInstall').hidden);
        assertFalse(page.$$('#isUnknownError').hidden);
      });
    });
  });
});
