// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_siminfo.m.js';

// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkSiminfoTest', function() {
  /** @type {!NetworkSiminfo|undefined} */
  let simInfo;

  setup(function() {
    simInfo = document.createElement('network-simInfo');
    document.body.appendChild(simInfo);
    Polymer.dom.flush();
  });

  test('Show SIM missing', function() {
    // SIM missing UI is dependent on the device state being set.
    let simMissingGroup = simInfo.$$('#simMissing');
    assertTrue(simMissingGroup.hidden);

    // SIM lock status is not set on the device state, so the SIM is considered
    // missing.
    simInfo.deviceState = {};
    Polymer.dom.flush();
    assertFalse(simMissingGroup.hidden);

    // SIM lock status is set, so the SIM is not considered missing.
    simInfo.deviceState = {
      simLockStatus: {}
    };
    Polymer.dom.flush();
    assertTrue(simMissingGroup.hidden);
  });
});
