// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_icon.js';

import {HotspotState} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ActivationStateType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkIconTest', function() {
  /** @type {!NetworkList|undefined} */
  let networkIcon;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function init() {
    networkIcon = document.createElement('network-icon');
    document.body.appendChild(networkIcon);
    assertTrue(!!networkIcon);
    flush();
  }

  test('Display locked cellular icon', async function() {
    init();
    const networkState =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    networkState.typeState.cellular.iccid = '1';
    networkState.typeState.cellular.eid = '1';
    networkState.typeState.cellular.simLocked = true;
    networkIcon.networkState = networkState;

    networkIcon.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [
        {slot_id: 1, eid: '1', iccid: '1', isPrimary: false},
      ],
      scanning: true,
    };
    await flushAsync();

    assertTrue(networkIcon.$$('#icon').classList.contains('cellular-locked'));
  });

  test('Display locked cellular icon for  carrier lock', async function() {
    init();
    const networkState =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    networkState.typeState.cellular.iccid = '1';
    networkState.typeState.cellular.eid = '1';
    networkState.typeState.cellular.simLocked = true;
    networkState.typeState.cellular.simLockType = 'network-pin';
    networkIcon.networkState = networkState;

    networkIcon.deviceState = {
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      simInfos: [
        {slot_id: 1, eid: '1', iccid: '1', isPrimary: false},
      ],
      scanning: true,
    };
    await flushAsync();

    assertTrue(
        networkIcon.$$('#icon').classList.contains('cellular-carrier-locked'));
  });

  [true, false].forEach(isUserLoggedIn => {
    test('Display unactivated PSim icon', async function() {
      loadTimeData.overrideValues({
        'isUserLoggedIn': isUserLoggedIn,
      });
      init();
      const networkState =
          OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
      networkState.typeState.cellular.iccid = '1';
      networkState.typeState.cellular.simLocked = false;
      networkState.typeState.cellular.activationState =
          ActivationStateType.kNotActivated;
      networkIcon.networkState = networkState;

      await flushAsync();

      if (!isUserLoggedIn) {
        assertTrue(networkIcon.$$('#icon').classList.contains(
            'cellular-not-activated'));
      } else {
        assertTrue(networkIcon.$$('#icon').classList.contains(
            'cellular-not-connected'));
      }
    });
  });

  test('Display roaming badge', async function() {
    init();
    const networkState =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    networkState.typeState.cellular.roaming = true;
    networkIcon.networkState = networkState;

    await flushAsync();

    assertFalse(networkIcon.$$('#roaming').hidden);
  });

  test('Should not display roaming badge', async function() {
    init();
    const networkState =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    networkState.typeState.cellular.roaming = false;
    networkIcon.networkState = networkState;

    await flushAsync();

    assertTrue(networkIcon.$$('#roaming').hidden);
  });

  test('Should not display badges for hotspot', async function() {
    init();
    const hotspotInfo = {state: HotspotState.kEnabled};
    networkIcon.hotspotInfo = hotspotInfo;

    await flushAsync();

    assertTrue(networkIcon.$$('#roaming').hidden);
    assertTrue(networkIcon.$$('#secure').hidden);
    assertTrue(networkIcon.$$('#technology').hidden);
  });

  [HotspotState.kEnabled, HotspotState.kDisabled, HotspotState.kEnabling,
   HotspotState.kDisabling]
      .forEach(hotspotState => {
        test('Should display icon for hotspot', async function() {
          init();
          const hotspotInfo = {state: hotspotState};
          networkIcon.hotspotInfo = hotspotInfo;

          await flushAsync();

          if (hotspotState === HotspotState.kEnabled) {
            assertTrue(
                networkIcon.$$('#icon').classList.contains('hotspot-on'));
          } else if (hotspotState === HotspotState.kEnabling) {
            assertTrue(networkIcon.$$('#icon').classList.contains(
                'hotspot-connecting'));
          } else {
            assertTrue(
                networkIcon.$$('#icon').classList.contains('hotspot-off'));
          }
        });
      });

  test('Should not display icon', async function() {
    init();
    const networkState =
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular');
    networkIcon.networkState = networkState;
    await flushAsync();

    let icon = networkIcon.$$('#icon');
    let tech_badge = networkIcon.$$('#technology');
    let secure_badge = networkIcon.$$('#secure');
    let roaming_badge = networkIcon.$$('#roaming');
    assertTrue(!!icon);
    assertTrue(!!tech_badge);
    assertTrue(!!secure_badge);
    assertTrue(!!roaming_badge);

    networkIcon.networkState = null;
    await flushAsync();

    icon = networkIcon.$$('#icon');
    tech_badge = networkIcon.$$('#technology');
    secure_badge = networkIcon.$$('#secure');
    roaming_badge = networkIcon.$$('#roaming');
    assertFalse(!!icon);
    assertFalse(!!tech_badge);
    assertFalse(!!secure_badge);
    assertFalse(!!roaming_badge);
  });
});
