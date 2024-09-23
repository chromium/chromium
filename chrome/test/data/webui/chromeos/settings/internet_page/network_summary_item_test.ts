// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrToggleElement, NetworkSummaryItemElement} from 'chrome://os-settings/os_settings.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {InhibitReason} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType, PortalState} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestInternetPageBrowserProxy} from './test_internet_page_browser_proxy.js';

suite('<network-summary-item>', () => {
  let netSummaryItem: NetworkSummaryItemElement;

  /**
   * Checks if the element exists and has not been 'removed' by the Polymer
   * template system.
   */
  function doesElementExist(selector: string): boolean {
    const el = netSummaryItem.shadowRoot!.querySelector<HTMLElement>(selector);
    return (el !== null) && (el.style.display !== 'none');
  }

  function initWithPSimOnly(isLocked: boolean, isDeviceEnabled = true) {
    const kTestIccid1 = '00000000000000000000';

    const simLockStatus = isLocked ? {lockType: 'sim-pin'} : {lockType: ''};

    netSummaryItem.setProperties({
      deviceState: {
        deviceState: isDeviceEnabled ? DeviceStateType.kEnabled :
                                       DeviceStateType.kDisabled,
        type: NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: simLockStatus,
        simInfos: [{slot_id: 1, eid: '', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
  }

  function initWithESimLocked() {
    const kTestIccid1 = '00000000000000000000';

    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
        simLockStatus: {lockType: 'sim-pin'},
        simInfos:
            [{slot_id: 1, eid: 'eid', iccid: kTestIccid1, isPrimary: true}],
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
  }

  setup(() => {
    netSummaryItem = document.createElement('network-summary-item');
    document.body.appendChild(netSummaryItem);
    flush();
  });

  teardown(() => {
    netSummaryItem.remove();
  });

  test('Device enabled button state', () => {
    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kUninitialized,
        type: NetworkType.kEthernet,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kEthernet,
      },
    });

    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kVPN,
    } as OncMojo.DeviceStateProperties;
    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kTether,
    } as OncMojo.DeviceStateProperties;
    flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.deviceState = {
      deviceState: DeviceStateType.kUninitialized,
      type: NetworkType.kWiFi,
    } as OncMojo.DeviceStateProperties;
    flush();
    assertFalse(doesElementExist('#deviceEnabledButton'));

    netSummaryItem.setProperties({
      activeNetworkState: {
        connectionState: ConnectionStateType.kConnected,
        guid: '',
        type: NetworkType.kWiFi,
        typeState: {
          wifi: {},
        },
      },
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kWiFi,
      },
    });
    flush();
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('Inhibited device on cellular network', () => {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kInstallingProfile,
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);
    assertEquals(
        netSummaryItem.i18n('internetDeviceBusy'),
        netSummaryItem['getNetworkStateText_']());
  });

  test('Not inhibited device on cellular network', () => {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kNotInhibited,
        deviceState: DeviceStateType.kDisabled,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);
  });

  test('Cellular modem flashing operation', () => {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kNotInhibited,
        deviceState: DeviceStateType.kDisabled,
        isFlashing: true,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);

    const networkStateText =
        netSummaryItem.shadowRoot!.querySelector<HTMLElement>('#networkState');
    assertTrue(!!networkStateText);
    assertEquals(
        netSummaryItem.i18n('internetDeviceFlashing'),
        networkStateText.textContent!.trim());


    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kNotInhibited,
        deviceState: DeviceStateType.kDisabled,
        isFlashing: false,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();

    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);

    const newNetworkStateText =
        netSummaryItem.shadowRoot!.querySelector<HTMLElement>('#networkState');
    assertTrue(!!newNetworkStateText);
    assertEquals(
        netSummaryItem.i18n('deviceOff'),
        newNetworkStateText.textContent!.trim());
  });

  test('Toggle should be disabled when device state is unavailable', () => {
    netSummaryItem.setProperties({
      deviceState: {
        inhibitReason: InhibitReason.kNotInhibited,
        deviceState: DeviceStateType.kUnavailable,
        type: NetworkType.kCellular,
        simAbsent: false,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });

    flush();
    assertFalse(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);
  });

  test('Toggle should be on and disabled when device state is enabling', () => {
    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kEnabling,
        type: NetworkType.kWiFi,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kWiFi,
        typeState: {
          wifi: {},
        },
      },
    });

    flush();
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.checked);
    assertTrue(
        netSummaryItem.shadowRoot!
            .querySelector<CrToggleElement>('#deviceEnabledButton')!.disabled);
  });

  test('Mobile data toggle shown on locked device', () => {
    initWithESimLocked();
    assert(netSummaryItem.shadowRoot!.querySelector('#deviceEnabledButton'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  // Regression test for b/264181192.
  test('pSIM-only locked device enabled, no SIM locked UI', () => {
    initWithPSimOnly(/*isLocked=*/ true);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                    .classList.contains('warning-message'));
    assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  // Regression test for b/264181192.
  test('pSIM-only locked device disabled, no SIM locked UI', () => {
    initWithPSimOnly(/*isLocked=*/ true, /*isDeviceEnabled=*/ false);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                    .classList.contains('warning-message'));
    assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('pSIM-only unlocked device enabled, no SIM locked UI', () => {
    initWithPSimOnly(/*isLocked=*/ false);
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                    .classList.contains('warning-message'));
    assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('eSIM enabled locked device, no SIM locked UI', () => {
    initWithESimLocked();
    assertFalse(doesElementExist('network-siminfo'));
    assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                    .classList.contains('warning-message'));
    assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                   .classList.contains('network-state'));
    assertTrue(doesElementExist('#deviceEnabledButton'));
  });

  test('Show networks list when only 1 pSIM network is available', async () => {
    const showNetworksFiredPromise =
        eventToPromise('show-networks', netSummaryItem);

    // Simulate a device which has a single pSIM slot and no eSIM slots.
    const simInfos = [{slotId: 1, iccid: '000', isPrimary: true, eid: ''}];

    netSummaryItem.setProperties({
      deviceState: {
        deviceState: DeviceStateType.kEnabled,
        type: NetworkType.kCellular,
        simAbsent: false,
        inhibitReason: InhibitReason.kNotInhibited,
        simLockStatus: {lockEnabled: false},
        simInfos: simInfos,
      },
      activeNetworkState: {
        connectionState: ConnectionStateType.kNotConnected,
        guid: '',
        type: NetworkType.kCellular,
        typeState: {cellular: {networkTechnology: ''}},
      },
    });
    flush();
    const networkState =
        netSummaryItem.shadowRoot!.querySelector<HTMLElement>('#networkState');
    assert(networkState);
    networkState.click();
    flush();
    await showNetworksFiredPromise;
  });

  suite('Portal', () => {
    const testName = 'test_name';
    const testGuid = '0001';
    let browserProxy: TestInternetPageBrowserProxy;

    function initWithPortalState(portalState: PortalState) {
      browserProxy = new TestInternetPageBrowserProxy();
      netSummaryItem.set('browserProxy_', browserProxy);

      netSummaryItem.setProperties({
        deviceState: {
          deviceState: DeviceStateType.kEnabled,
          inhibitReason: InhibitReason.kNotInhibited,
          type: NetworkType.kWiFi,
        },
        activeNetworkState: {
          connectionState: ConnectionStateType.kPortal,
          guid: testGuid,
          type: NetworkType.kWiFi,
          typeState: {
            wifi: {
              bssid: 'bssid',
              frequency: 1,
              hexSsid: 'hexSsid',
              security: 'security',
              signalStrength: 99,
              ssid: 'ssid',
              hiddenSsid: false,
            },
          },
          name: testName,
          portalState: portalState,
        },
      });
      flush();
    }

    test(
        'kPortal shows signin text and opens portal signin on click',
        async () => {
          initWithPortalState(PortalState.kPortal);
          assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                         .classList.contains('warning-message'));
          assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                          .classList.contains('network-state'));
          assertEquals(
              netSummaryItem.i18n('networkListItemSignIn'),
              netSummaryItem['getNetworkStateText_']());
          assertEquals(testName, netSummaryItem['getTitleText_']());

          // Verify clicking network summary item will open portal signin
          const networkSummaryItemRow =
              netSummaryItem.shadowRoot!.querySelector<HTMLElement>(
                  '#networkSummaryItemRow');
          assert(networkSummaryItemRow);
          networkSummaryItemRow.click();
          const guid = await browserProxy.whenCalled('showPortalSignin');
          assertEquals(1, browserProxy.getCallCount('showPortalSignin'));
          assertEquals(testGuid, guid);
        });

    test(
        'kPortal shows signin text and opens network list on arrow click',
        async () => {
          const showNetworksFiredPromise =
              eventToPromise('show-networks', netSummaryItem);

          initWithPortalState(PortalState.kPortal);
          assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                         .classList.contains('warning-message'));
          assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                          .classList.contains('network-state'));
          assertEquals(
              netSummaryItem.i18n('networkListItemSignIn'),
              netSummaryItem['getNetworkStateText_']());
          assertEquals(testName, netSummaryItem['getTitleText_']());

          // Verify clicking network summary item arrow icon will show networks
          const networkSummaryItemRowArrowIcon =
              netSummaryItem.shadowRoot!.querySelector<HTMLElement>(
                  '#networkSummaryItemRowArrowIcon');
          assert(networkSummaryItemRowArrowIcon);
          networkSummaryItemRowArrowIcon.click();
          flush();
          await showNetworksFiredPromise;
        });

    test(
        'kPortalSuspected shows signin text and opens portal signin on click',
        async () => {
          initWithPortalState(PortalState.kPortalSuspected);
          assertTrue(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                         .classList.contains('warning-message'));
          assertFalse(netSummaryItem.shadowRoot!.querySelector('#networkState')!
                          .classList.contains('network-state'));
          assertEquals(
              netSummaryItem.i18n('networkListItemSignIn'),
              netSummaryItem['getNetworkStateText_']());
          assertEquals(testName, netSummaryItem['getTitleText_']());

          // Verify clicking network summary item will open portal signin
          const networkSummaryItemRow =
              netSummaryItem.shadowRoot!.querySelector<HTMLElement>(
                  '#networkSummaryItemRow');
          assert(networkSummaryItemRow);
          networkSummaryItemRow.click();
          const guid = await browserProxy.whenCalled('showPortalSignin');
          assertEquals(1, browserProxy.getCallCount('showPortalSignin'));
          assertEquals(testGuid, guid);
        });

    test('Error message displayed when Bluetooth is disabled', () => {
      netSummaryItem.setProperties({
        deviceState: {
          inhibitReason: InhibitReason.kNotInhibited,
          deviceState: DeviceStateType.kUninitialized,
          type: NetworkType.kTether,
        },
        activeNetworkState: {
          connectionState: ConnectionStateType.kNotConnected,
          guid: '',
          type: NetworkType.kTether,
        },
      });

      flush();
      assertEquals(
          netSummaryItem.i18n('tetherEnableBluetooth'),
          netSummaryItem['getNetworkStateText_']());

      netSummaryItem.setProperties({
        deviceState: {
          inhibitReason: InhibitReason.kNotInhibited,
          deviceState: DeviceStateType.kEnabled,
          type: NetworkType.kTether,
        },
        activeNetworkState: {
          connectionState: ConnectionStateType.kNotConnected,
          guid: '',
          type: NetworkType.kTether,
        },
      });

      flush();
      assertEquals(
          netSummaryItem.i18n('networkListItemNoNetwork'),
          netSummaryItem['getNetworkStateText_']());
    });
  });
});
