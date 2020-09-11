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

  setup(function() {
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
});
