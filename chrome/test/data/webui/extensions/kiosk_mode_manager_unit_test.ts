// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import {ExtensionsManagerElement, KioskBrowserProxyImpl, Service} from 'chrome://extensions/extensions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestKioskBrowserProxy} from './test_kiosk_browser_proxy.js';
import {TestService} from './test_service.js';

const extension_manager_unit_tests = {
  suiteName: 'ExtensionManagerUnitTest',
  TestNames: {
    KioskMode: 'kiosk mode',
  },
};

Object.assign(window, {extension_manager_unit_tests});

suite(extension_manager_unit_tests.suiteName, function() {
  let manager: ExtensionsManagerElement;

  let service: TestService;

  let browserProxy: TestKioskBrowserProxy;

  setup(function() {
    browserProxy = new TestKioskBrowserProxy();
    KioskBrowserProxyImpl.setInstance(browserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    service = new TestService();
    Service.setInstance(service);

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait until Manager calls fetches data and initializes itself before
    // making any assertions.
    return Promise.all([
      service.whenCalled('getExtensionsInfo'),
      service.whenCalled('getProfileConfiguration'),
    ]);
  });

  test(
      extension_manager_unit_tests.TestNames.KioskMode, async function() {
        assertFalse(
            !!manager.shadowRoot!.querySelector('extensions-kiosk-dialog'));

        await browserProxy.whenCalled('initializeKioskAppSettings');
        assertTrue(manager.shadowRoot!.querySelector(
                                          'extensions-toolbar')!.kioskEnabled);
        manager.shadowRoot!.querySelector('extensions-toolbar')!.dispatchEvent(
            new CustomEvent('kiosk-tap', {bubbles: true, composed: true}));
        flush();
        assertTrue(
            !!manager.shadowRoot!.querySelector('extensions-kiosk-dialog'));
      });
});
