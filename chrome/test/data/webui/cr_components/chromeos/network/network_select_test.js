// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/cr_components/chromeos/network/network_select.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkSelectTest', function() {
  /** @type {!NetworkSelect|undefined} */
  let networkSelect;

  setup(async function() {
    // The OOBE host uses polyfill which requires the test to wait until HTML
    // imports have finished loading before initiating any tests. The Polymer 3
    // version of the test does not use the OOBE host so this line should not
    // execute.
    /* #ignore */ await cr.ui.Oobe.waitForOobeToLoad();
    networkSelect = document.createElement('network-select');
    document.body.appendChild(networkSelect);
    Polymer.dom.flush();
  });

  test('Scan progress visibility', function() {
    // Scan progress is not shown by default.
    let paperProgress = networkSelect.$$('paper-progress');
    assertEquals(null, paperProgress);
    assertFalse(networkSelect.showScanProgress);

    networkSelect.showScanProgress = true;
    Polymer.dom.flush();

    paperProgress = networkSelect.$$('paper-progress');
    assertTrue(!!paperProgress);
  });

  test('Disable Wi-Fi scan', function() {
    // When |networkSelect| is attached to the DOM, it should schedule periodic
    // Wi-Fi scans.
    assertTrue(networkSelect.scanIntervalId_ !== null);

    // Setting |enableWifiScans| to false should clear the scheduled scans.
    networkSelect.enableWifiScans = false;
    Polymer.dom.flush();
    assertTrue(networkSelect.scanIntervalId_ === null);

    // Setting |enableWifiScans| back to true should re-schedule them.
    networkSelect.enableWifiScans = true;
    Polymer.dom.flush();
    assertTrue(networkSelect.scanIntervalId_ !== null);
  });
});
