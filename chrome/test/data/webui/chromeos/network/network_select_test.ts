// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_select.js';

import {NetworkSelectElement} from 'chrome://resources/ash/common/network/network_select.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkSelectTest', function() {
  let networkSelect: NetworkSelectElement|undefined;

  setup(async function() {
    networkSelect = document.createElement('network-select');
    document.body.appendChild(networkSelect);
    flush();
  });

  test('Scan progress visibility', function() {
    assertTrue(!!networkSelect);

    // Scan progress is not shown by default.
    let paperProgress =
        networkSelect.shadowRoot!.querySelector('paper-progress');
    assertEquals(null, paperProgress);
    assertFalse(networkSelect.showScanProgress);

    networkSelect.showScanProgress = true;
    flush();

    paperProgress = networkSelect.shadowRoot!.querySelector('paper-progress');
    assertTrue(!!paperProgress);
  });

  test('Disable Wi-Fi scan', function() {
    assertTrue(!!networkSelect);

    // When |networkSelect| is attached to the DOM, it should schedule periodic
    // Wi-Fi scans.
    assertTrue(networkSelect.isScanning());

    // Setting |enableWifiScans| to false should clear the scheduled scans.
    networkSelect.enableWifiScans = false;
    flush();
    assertTrue(networkSelect.isScanning());

    // Setting |enableWifiScans| back to true should re-schedule them.
    networkSelect.enableWifiScans = true;
    flush();
    assertTrue(networkSelect.isScanning());
  });
});
