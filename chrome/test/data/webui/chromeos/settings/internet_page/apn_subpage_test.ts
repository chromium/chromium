// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {ApnSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {ApnListElement} from 'chrome://resources/ash/common/network/apn_list.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {DeviceStateProperties, InhibitReason, ManagedProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {DeviceStateType, NetworkType, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite('<apn-subpage>', () => {
  let apnSubpage: ApnSubpageElement;
  let mojoApi_: FakeNetworkConfig;

  function getDefaultDeviceStateProps(): DeviceStateProperties {
    return {
      ipv4Address: undefined,
      ipv6Address: undefined,
      imei: undefined,
      macAddress: undefined,
      scanning: false,
      simLockStatus: undefined,
      simInfos: undefined,
      inhibitReason: InhibitReason.kNotInhibited,
      simAbsent: false,
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kCellular,
      managedNetworkAvailable: false,
      serial: undefined,
      isCarrierLocked: false,
      isFlashing: false,
    };
  }

  function getApnList(): ApnListElement|null {
    return apnSubpage.shadowRoot!.querySelector('apn-list');
  }

  setup(async () => {
    clearBody();
    mojoApi_ = new FakeNetworkConfig();
    mojoApi_.setNetworkTypeEnabledState(NetworkType.kCellular, true);
    mojoApi_.setManagedPropertiesForTest(OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular'));
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);

    // Start at BASIC route so that if the APN subpage navigates backwards, it
    // goes back to BASIC and not the previous tests' last page (crbug/1497312).
    Router.getInstance().navigateTo(routes.BASIC);
    await flushTasks();

    apnSubpage = document.createElement('apn-subpage');
    document.body.appendChild(apnSubpage);
    const params = new URLSearchParams();
    params.append('guid', 'cellular_guid');
    Router.getInstance().navigateTo(routes.APN, params);

    return flushTasks();
  });

  teardown(() => {
    return flushTasks().then(() => {
      apnSubpage.remove();
      Router.getInstance().resetRouteForTesting();
    });
  });

  test('Check if APN list exists', async () => {
    assertTrue(!!apnSubpage);
    assertTrue(!!getApnList());
  });

  test('Page closed while device is updating', async () => {
    const oldClose = apnSubpage.close;
    let counter = 0;
    apnSubpage.close = () => {
      counter++;
      oldClose.apply(apnSubpage);
    };
    mojoApi_.beforeGetDeviceStateList = () => {
      apnSubpage.close();
    };
    mojoApi_.setDeviceStateForTest({
      ...getDefaultDeviceStateProps(),
      type: NetworkType.kCellular,
      deviceState: DeviceStateType.kEnabled,
      scanning: true,
    });
    await flushTasks();
    assertEquals(1, counter);
    assertEquals(undefined, apnSubpage.get('managedProperties_'));
  });

  test('Page closed while network is updating', async () => {
    const oldClose = apnSubpage.close;
    let counter = 0;
    apnSubpage.close = () => {
      counter++;
      oldClose.apply(apnSubpage);
    };
    mojoApi_.beforeGetManagedProperties = () => {
      apnSubpage.close();
    };
    mojoApi_.setManagedPropertiesForTest(OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular'));
    await flushTasks();
    assertEquals(1, counter);
    assertEquals(undefined, apnSubpage.get('managedProperties_'));
  });

  test(
      'Only updating with different device state should call networkDetails',
      async () => {
        let counter = 0;
        const oldGetNetworkDetails = apnSubpage.get('getNetworkDetails_');
        apnSubpage.set('getNetworkDetails_', () => {
          counter++;
          oldGetNetworkDetails.apply(apnSubpage);
        });
        mojoApi_.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n1',
        });
        await flushTasks();
        // The new device state has the same type as the old, but it is not a
        // full match - we won't call getNetworkDetails.
        assertEquals(0, counter);
        mojoApi_.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n1',
        });
        await flushTasks();
        // The new device state has the same type as the old and they fully
        // match - we need to call getNetworkDetails. This is because for
        // cellular networks, some shill device level properties are
        // represented at network level in ONC.
        assertEquals(1, counter);
        mojoApi_.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kDisabled,
          macAddress: 'n2',
        });
        await flushTasks();
        // The new device state is not a full match - we would call
        // getNetworkDetails, because the deviceState has changed.
        assertEquals(2, counter);
        mojoApi_.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n2',
        });
        await flushTasks();
        // The new device state is not a full match - we would call
        // getNetworkDetails, because the deviceState has changed.
        assertEquals(3, counter);
        mojoApi_.setDeviceStateForTest({
          ...getDefaultDeviceStateProps(),
          type: NetworkType.kCellular,
          deviceState: DeviceStateType.kEnabled,
          macAddress: 'n2',
          scanning: true,
        });
        await flushTasks();
        // When scanning and type is cellular we always call getNetworkDetails
        assertEquals(4, counter);
      });

  test('Error state is propagated to <apn-list>', async () => {
    let props: ManagedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    assertEquals(undefined, getApnList()!.errorState);

    const error = 'connect-failed';
    props = {
      ...props,
      errorState: error,
    };

    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    assertEquals(error, getApnList()!.errorState);
  });

  test('Close is called more than once.', async () => {
    let counter = 0;
    const navigateToPreviousRoute =
        Router.getInstance().navigateToPreviousRoute;
    Router.getInstance().navigateToPreviousRoute = () => {
      counter++;
    };
    apnSubpage.close();
    await flushTasks();

    apnSubpage.close();
    await flushTasks();

    assertEquals(1, counter);
    Router.getInstance().navigateToPreviousRoute = navigateToPreviousRoute;
  });

  test('Network removed while on subpage', async () => {
    let counter = 0;
    const navigateToPreviousRoute =
        Router.getInstance().navigateToPreviousRoute;
    Router.getInstance().navigateToPreviousRoute = () => {
      counter++;
    };

    // Simulate the network being removed.
    mojoApi_.resetForTest();
    apnSubpage.onNetworkStateChanged(
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular'));
    await flushTasks();

    assertEquals(1, counter);
    Router.getInstance().navigateToPreviousRoute = navigateToPreviousRoute;
  });

  test('Network removed while not on subpage', async () => {
    // Navigate to a different page.
    const params = new URLSearchParams();
    params.append('type', OncMojo.getNetworkTypeString(NetworkType.kCellular));
    Router.getInstance().navigateTo(routes.INTERNET_NETWORKS, params);
    await flushTasks();
    assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);

    let counter = 0;
    const navigateToPreviousRoute =
        Router.getInstance().navigateToPreviousRoute;
    Router.getInstance().navigateToPreviousRoute = () => {
      counter++;
    };

    // Simulate the network being removed.
    mojoApi_.resetForTest();
    apnSubpage.onNetworkStateChanged(
        OncMojo.getDefaultNetworkState(NetworkType.kCellular, 'cellular'));
    await flushTasks();

    assertEquals(0, counter);
    Router.getInstance().navigateToPreviousRoute = navigateToPreviousRoute;
  });

  test('Portal state is propagated to <apn-list>', async () => {
    let props: ManagedProperties = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, 'cellular_guid', 'cellular');
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    assertEquals(PortalState.kUnknown, getApnList()!.portalState);

    props = {
      ...props,
      portalState: PortalState.kNoInternet,
    };
    mojoApi_.setManagedPropertiesForTest(props);
    await flushTasks();
    assertEquals(PortalState.kNoInternet, getApnList()!.portalState);
  });

  test('ShouldDisallowApnModification propagated to <apn-list>', async () => {
    assertFalse(getApnList()!.shouldDisallowApnModification);
    apnSubpage.shouldDisallowApnModification = true;
    assertTrue(getApnList()!.shouldDisallowApnModification);
  });
});
