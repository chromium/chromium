// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/strings.m.js';
import 'chrome://resources/ash/common/network/network_choose_mobile.js';

import type {NetworkChooseMobileElement} from 'chrome://resources/ash/common/network/network_choose_mobile.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('NetworkChooseMobileTest', () => {
  let chooseMobile: NetworkChooseMobileElement|undefined;

  function getDeviceState(scanning: boolean): OncMojo.DeviceStateProperties {
    return {
      deviceState: DeviceStateType.kEnabled,
      type: NetworkType.kCellular,
      scanning: scanning,
    } as OncMojo.DeviceStateProperties;
  }

  setup(() => {
    chooseMobile = document.createElement('network-choose-mobile');
    chooseMobile.managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, /* guid= */ 'cellular1', /* name= */ 'foo');
    document.body.appendChild(chooseMobile);
    flush();
  });

  test('Scan button enabled state', () => {
    assertTrue(!!chooseMobile);

    const scanButton = chooseMobile.shadowRoot!.querySelector('cr-button');
    assertTrue(!!scanButton);
    assertTrue(scanButton.disabled);

    // A scan requires the connection state to be disconnected and the current
    // scan state to be 'not scanning'.
    chooseMobile.deviceState = getDeviceState(/* scanning= */ false);
    flush();

    // Scan button is enabled.
    const isScanEnabled = !scanButton.disabled;
    assertTrue(isScanEnabled);

    // Set the device state to scanning.
    chooseMobile.deviceState = getDeviceState(/* scanning= */ true);
    flush();

    // Scan button is disabled while the device is currently scanning.
    assertTrue(scanButton.disabled);

    // Reset scanning status.
    chooseMobile.deviceState = getDeviceState(/* scanning= */ false);

    for (const state
             of [ConnectionStateType.kOnline, ConnectionStateType.kConnected,
                 ConnectionStateType.kPortal, ConnectionStateType.kConnecting,
                 ConnectionStateType.kNotConnected]) {
      const managedProperties = OncMojo.getDefaultManagedProperties(
          NetworkType.kCellular, /* guid= */ 'cellular1', /* name= */ 'foo');
      managedProperties.connectionState = state;
      chooseMobile.managedProperties = managedProperties;
      flush();

      // Every connection state but kNotConnected should prevent scanning.
      if (state === ConnectionStateType.kNotConnected) {
        assertFalse(scanButton.disabled);
      } else {
        assertTrue(scanButton.disabled);
      }
    }
  });

  test('Disabled UI state', () => {
    assertTrue(!!chooseMobile);

    const managedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, /* guid= */ 'cellular1', /* name= */ 'foo');
    managedProperties.typeProperties!.cellular!.foundNetworks = [{
      status: '',
      networkId: '1',
      technology: '',
      longName: 'network_name',
      shortName: null,
    }];
    chooseMobile.managedProperties = managedProperties;
    chooseMobile.deviceState = getDeviceState(/* scanning= */ false);
    flush();

    const scanButton = chooseMobile.shadowRoot!.querySelector('cr-button');
    assertTrue(!!scanButton);
    const select = chooseMobile.shadowRoot!.querySelector('select');
    assertTrue(!!select);

    assertFalse(scanButton.disabled);
    assertFalse(select.disabled);

    chooseMobile.disabled = true;

    assertTrue(scanButton.disabled);
    assertTrue(select.disabled);
  });
});
