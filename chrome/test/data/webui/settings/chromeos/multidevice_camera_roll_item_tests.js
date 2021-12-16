// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {SyncBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
// #import {TestSyncBrowserProxy} from './test_os_sync_browser_proxy.m.js';
// clang-format on

suite('Multidevice', function() {
  let cameraRollItem;

  setup(function() {
    const browserProxy = new TestSyncBrowserProxy();
    settings.SyncBrowserProxyImpl.setInstance(browserProxy);

    PolymerTest.clearBody();

    cameraRollItem =
        document.createElement('settings-multidevice-camera-roll-item');
    document.body.appendChild(cameraRollItem);

    Polymer.dom.flush();
  });

  teardown(function() {
    cameraRollItem.remove();
  });

  test(
      'Camera Roll toggle is disabled when file access is not granted',
      async () => {
        cameraRollItem.pageContentData =
            Object.assign({}, cameraRollItem.pageContentData, {
              isCameraRollFilePermissionGranted: false,
            });
        Polymer.dom.flush();

        assertTrue(!!cameraRollItem.$$('localized-link[slot=feature-summary]'));
        const toggle = cameraRollItem.$$('cr-toggle[slot=feature-controller]');
        assertTrue(!!toggle);
        assertTrue(toggle.disabled);

        cameraRollItem.pageContentData =
            Object.assign({}, cameraRollItem.pageContentData, {
              isCameraRollFilePermissionGranted: true,
            });
        Polymer.dom.flush();

        assertFalse(
            !!cameraRollItem.$$('localized-link[slot=feature-summary]'));
        assertFalse(!!cameraRollItem.$$('cr-toggle[slot=feature-controller]'));
      });
});
