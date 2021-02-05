// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for chrome://bluetooth-internals
 */

GEN('#include "content/public/test/browser_test.h"');

/**
 * Test fixture for BluetoothInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function BluetoothInternalsTest() {}

BluetoothInternalsTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://bluetooth-internals',

  /** @override */
  isAsync: true,

  /** @override */
  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/assert.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//ui/webui/resources/js/cr.js',
    '//ui/webui/resources/js/util.js',
    '//chrome/test/data/webui/test_browser_proxy.js',
    '//chrome/test/data/webui/bluetooth_internals/test_utils.js',
    '//chrome/test/data/webui/bluetooth_internals/bluetooth_internals_test.js',
  ],

  preLoad: function() {
    const resolver = new PromiseResolver();
    window.setupPromise = resolver.promise;
    window.setupFn = () => {
      let internalsHandlerInterceptor = new MojoInterfaceInterceptor(
          mojom.BluetoothInternalsHandler.$interfaceName);
      internalsHandlerInterceptor.oninterfacerequest = (e) => {
        window.internalsHandler = new TestBluetoothInternalsHandler(e.handle);

        const testAdapter = new TestAdapter(fakeAdapterInfo());
        testAdapter.setTestDevices([
          fakeDeviceInfo1(),
          fakeDeviceInfo2(),
        ]);

        const testServices = [fakeServiceInfo1(), fakeServiceInfo2()];

        testAdapter.setTestServicesForTestDevice(
            fakeDeviceInfo1(), Object.assign({}, testServices));
        testAdapter.setTestServicesForTestDevice(
            fakeDeviceInfo2(), Object.assign({}, testServices));

        window.internalsHandler.setAdapterForTesting(testAdapter);
        resolver.resolve();
      };
      internalsHandlerInterceptor.start();
      return Promise.resolve();
    };
  }
};

TEST_F('BluetoothInternalsTest', 'Startup_BluetoothInternals', function() {
  // Run all registered tests.
  mocha.run();
});
