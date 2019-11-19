// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for extension-manager unit tests. Unlike
 * extension_manager_test.js, these tests are not interacting with the real
 * chrome.developerPrivate API.
 */

import {KioskBrowserProxyImpl, Service} from 'chrome://extensions/extensions.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {TestKioskBrowserProxy} from './test_kiosk_browser_proxy.js';
import {TestService} from './test_service.js';

window.extension_manager_unit_tests = {};
extension_manager_unit_tests.suiteName = 'ExtensionManagerUnitTest';

/** @enum {string} */
extension_manager_unit_tests.TestNames = {
  KioskMode: 'kiosk mode',
};

suite(extension_manager_unit_tests.suiteName, function() {
  /** @type {Manager} */
  let manager;

  /** @type {TestService} */
  let service;

  /** @type {KioskBrowserProxy} */
  let browserProxy;

  setup(function() {
    browserProxy = new TestKioskBrowserProxy();
    KioskBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    service = new TestService();
    Service.instance_ = service;

    manager = document.createElement('extensions-manager');
    document.body.appendChild(manager);

    // Wait until Manager calls fetches data and initializes itself before
    // making any assertions.
    return Promise.all([
      service.whenCalled('getExtensionsInfo'),
      service.whenCalled('getProfileConfiguration'),
    ]);
  });

  test(assert(extension_manager_unit_tests.TestNames.KioskMode), function() {
    expectFalse(!!manager.$$('extensions-kiosk-dialog'));

    return browserProxy.whenCalled('initializeKioskAppSettings').then(() => {
      expectTrue(manager.$$('extensions-toolbar').kioskEnabled);
      manager.$$('extensions-toolbar').fire('kiosk-tap');
      flush();
      expectTrue(!!manager.$$('extensions-kiosk-dialog'));
    });
  });
});
