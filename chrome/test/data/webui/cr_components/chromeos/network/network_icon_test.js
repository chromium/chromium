// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_icon.m.js';

// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkIconTest', function() {
  /** @type {!NetworkList|undefined} */
  let networkIcon;

  let mojom;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    loadTimeData.overrideValues({
      updatedCellularActivationUi: true,
    });
    mojom = chromeos.networkConfig.mojom;
    networkIcon = document.createElement('network-icon');
    document.body.appendChild(networkIcon);
    assertTrue(!!networkIcon);
    Polymer.dom.flush();
  });

  test('Display locked cellular icon', async function() {
    const networkState = OncMojo.getDefaultNetworkState(
      mojom.NetworkType.kCellular, 'cellular');
    networkState.typeState.cellular.iccid = '1';
    networkState.typeState.cellular.eid = '1';
    networkState.typeState.cellular.simLocked = true;
    networkIcon.networkState = networkState;

    networkIcon.deviceState = {
      type: mojom.NetworkType.kCellular,
      deviceState: mojom.DeviceStateType.kEnabled,
      simInfos: [
        {slot_id: 1, eid: '1', iccid: '1', isPrimary: false},
      ],
      scanning: true
    };
    await flushAsync();

    assertTrue(networkIcon.$$('#icon').classList.contains('cellular-locked'));
  });
});
