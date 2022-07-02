// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of CrosNetworkConfig for testing.
 */

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

// Default cellular pin, used when locking/unlocking cellular profiles.
export const DEFAULT_CELLULAR_PIN = '1111';

// TODO(stevenjb): Include cros_network_config.mojom.js and extend
// CrosNetworkConfigInterface
export class FakeNetworkConfig {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private {!Map<chromeos.networkConfig.mojom.NetworkType,
     *     !chromeos.networkConfig.mojom.DeviceStateProperties>}
     */
    this.deviceStates_ = new Map();

    /**
     * @private {!Array<!chromeos.networkConfig.mojom.NetworkStateProperties>}
     */
    this.networkStates_ = [];

    /**
     * @private {!Map<string, !chromeos.networkConfig.mojom.ManagedProperties>}
     */
    this.managedProperties_ = new Map();

    /** @private {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    this.globalPolicy_ = undefined;

    /** @private {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} */
    this.serverCas_ = [];

    /** @private {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} */
    this.userCerts_ = [];

    /**
     * @private {!Array<
     *     !chromeos.networkConfig.mojom.CrosNetworkConfigObserverRemote>}
     */
    this.observers_ = [];

    /**
     * When updating or changing cellular pin, |testPin| is used to store
     * the updated pin, if not set DEFAULT_CELLULAR_PIN is used to check pin
     * value in |setCellularSimState()|
     * @type {string}
     */
    this.testPin = '';

    /**
     * @private {chromeos.networkConfig.mojom.AlwaysOnVpnProperties}
     */
    this.alwaysOnVpnProperties_ = {
      mode: chromeos.networkConfig.mojom.AlwaysOnVpnMode.kOff,
      serviceGuid: '',
    };

    /** @type {Function} */
    this.beforeGetDeviceStateList = null;

    /** @private {!Array<chromeos.networkConfig.mojom.VpnProvider>} */
    this.vpnProviders_ = [];

    /** @private {!Map<string, !Array<!Object>>} */
    this.trafficCountersMap_ = new Map();

    /** @private {!Map<string, !Array<!Object>>} */
    this.autoResetValuesMap_ = new Map();

    this.resetForTest();
  }

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {chromeos.networkConfig.mojom.DeviceStateProperties}
   * @private
   */
  addDeviceState_(type) {
    assert(type !== undefined);
    const deviceState =
        /** @type {!chromeos.networkConfig.mojom.DeviceStateProperties} */ ({
          type: type,
          deviceState:
              chromeos.networkConfig.mojom.DeviceStateType.kUninitialized,
          inhibitReason:
              chromeos.networkConfig.mojom.InhibitReason.kNotInhibited
        });
    this.deviceStates_.set(type, deviceState);
    return deviceState;
  }

  resetForTest() {
    const mojom = chromeos.networkConfig.mojom;

    this.deviceStates_ = new Map();
    this.addDeviceState_(mojom.NetworkType.kEthernet).deviceState =
        chromeos.networkConfig.mojom.DeviceStateType.kEnabled;
    this.addDeviceState_(mojom.NetworkType.kWiFi);
    this.addDeviceState_(mojom.NetworkType.kCellular);
    this.addDeviceState_(mojom.NetworkType.kTether);
    this.addDeviceState_(mojom.NetworkType.kVPN);

    this.globalPolicy_ =
        /** @type {!chromeos.networkConfig.mojom.GlobalPolicy} */ ({
          allow_cellular_sim_lock: true,
          allow_only_policy_cellular_networks: false,
          allow_only_policy_networks_to_autoconnect: false,
          allow_only_policy_wifi_networks_to_connect: false,
          allow_only_policy_wifi_networks_to_connect_if_available: false,
          blocked_hex_ssids: [],
        });

    const eth0 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');
    this.networkStates_ = [eth0];

    this.managedProperties_ = new Map();

    this.vpnProviders_ = [];

    this.serverCas_ = [];
    this.userCerts_ = [];

    ['getNetworkState',
     'getNetworkStateList',
     'getDeviceStateList',
     'getManagedProperties',
     'setNetworkTypeEnabledState',
     'requestNetworkScan',
     'getGlobalPolicy',
     'getVpnProviders',
     'getNetworkCertificates',
     'setProperties',
     'setCellularSimState',
     'startConnect',
     'startDisconnect',
     'configureNetwork',
     'getAlwaysOnVpn',
     'getSupportedVpnTypes',
     'requestTrafficCounters',
     'resetTrafficCounters',
     'setTrafficCountersAutoReset',
    ].forEach((methodName) => {
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @param {string} methodName
   * @return {!PromiseResolver}
   * @private
   */
  getResolver_(methodName) {
    const method = this.resolverMap_.get(methodName);
    assert(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  /**
   * @param {string} methodName
   * @protected
   */
  methodCalled(methodName) {
    this.getResolver_(methodName).resolve();
  }

  /**
   * @param {string} methodName
   * @return {!Promise}
   */
  whenCalled(methodName) {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  /**
   * @param {!Array<!chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   */
  addNetworksForTest(networks) {
    this.networkStates_ = this.networkStates_.concat(networks);
    this.onNetworkStateListChanged();
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   */
  removeNetworkForTest(network) {
    this.networkStates_ = this.networkStates_.filter((state) => {
      return state.guid !== network.guid;
    });
    this.onNetworkStateListChanged();
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} network
   */
  setManagedPropertiesForTest(network) {
    assert(network.guid);
    this.managedProperties_.set(network.guid, network);

    const networkState = OncMojo.managedPropertiesToNetworkState(network);
    const idx = this.networkStates_.findIndex(state => {
      return state.guid === network.guid;
    });
    if (idx >= 0) {
      this.networkStates_[idx] = networkState;
      this.onNetworkStateChanged(networkState);
    } else {
      this.networkStates_.push(networkState);
    }
    this.onNetworkStateListChanged();
  }

  /**
   * @param {string} guid
   * @param {chromeos.networkConfig.mojom.ConnectionStateType} state
   */
  setNetworkConnectionStateForTest(guid, state) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    network.connectionState = state;

    const managed = this.managedProperties_.get(guid);
    if (managed) {
      managed.connectionState = state;
    }
    this.onActiveNetworksChanged();
  }

  /**
   * @param {string} guid
   * @param {!Array<!Object>} trafficCounters counters for guid
   */
  setTrafficCountersForTest(guid, trafficCounters) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);

    this.trafficCountersMap_.set(guid, trafficCounters);
  }

  /**
   * @param {string} guid
   * @param {?mojoBase.mojom.Time} lastResetTime last reset
   * time for network with guid
   */
  setLastResetTimeForTest(guid, lastResetTime) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    const managed = this.managedProperties_.get(guid);
    if (managed) {
      assert(
          !!managed.trafficCounterProperties,
          'Missing traffic counter properties for network: ' + guid);
      managed.trafficCounterProperties.lastResetTime = lastResetTime;
    }
    this.onActiveNetworksChanged();
  }

  /**
   * @param {string} guid
   * @param {string} friendlyDate a human readable date representing
   * the last reset time
   *
   */
  setFriendlyDateForTest(guid, friendlyDate) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    const managed = this.managedProperties_.get(guid);
    if (managed) {
      assert(
          !!managed.trafficCounterProperties,
          'Missing traffic counter properties for network: ' + guid);
      managed.trafficCounterProperties.friendlyDate = friendlyDate;
    }
    this.onActiveNetworksChanged();
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.StartConnectResult}>}
   */
  startConnect(guid) {
    return new Promise(resolve => {
      this.methodCalled('startConnect');
      resolve(
          {result: chromeos.networkConfig.mojom.StartConnectResult.kCanceled});
    });
  }

  /**
   * @param { !string } guid
   * @return {!Promise<{
        success: !boolean,
   *  }>}
   */
  startDisconnect(guid) {
    return new Promise(resolve => {
      const network = this.networkStates_.find(state => {
        return state.guid === guid;
      });
      network.connectionState =
          chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
      this.methodCalled('startDisconnect');
      resolve({success: true});
    });
  }

  /**
   * @param {chromeos.networkConfig.mojom.ConfigProperties} properties
   * @param {boolean} shared
   * @return {!Promise<{guid: string, errorMessage: string}>}
   */
  configureNetwork(properties, shared) {
    return new Promise(resolve => {
      this.methodCalled('configureNetwork');
      resolve({guid: 'test_guid', errorMessage: ''});
    });
  }

  /**
   * @param {string} guid
   * @param {chromeos.networkConfig.mojom.ConfigProperties} properties
   * @return {!Promise<{success: boolean, errorMessage: string}>}
   */
  setProperties(guid, properties) {
    return new Promise(resolve => {
      this.methodCalled('setProperties');
      resolve({success: true, errorMessage: ''});
    });
  }

  /**
   * @param {chromeos.networkConfig.mojom.DeviceStateProperties} deviceState
   * @private
   */
  setDeviceStateForTest(deviceState) {
    assert(deviceState.type !== undefined);
    this.deviceStates_.set(deviceState.type, deviceState);
    this.onDeviceStateListChanged();
  }

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {?chromeos.networkConfig.mojom.DeviceStateProperties}
   */
  getDeviceStateForTest(type) {
    return this.deviceStates_.get(type) || null;
  }

  /** @param {!Array<!chromeos.networkConfig.mojom.VpnProvider>} providers */
  setVpnProvidersForTest(providers) {
    this.vpnProviders_ = providers;
    this.onVpnProvidersChanged();
  }

  /**
   * @param {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} serverCas
   * @param {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} userCerts
   */
  setCertificatesForTest(serverCas, userCerts) {
    this.serverCas_ = serverCas;
    this.userCerts_ = userCerts;
    this.onNetworkCertificatesChanged();
  }

  // networkConfig observers
  onActiveNetworksChanged() {
    const activeNetworks = this.networkStates_.filter(state => {
      // Calling onActiveNetworksChanged will trigger mojo checks on all
      // NetworkStateProperties. Ensure they were created using
      // OncMojo.getDefaultNetworkState.
      if (state.connectable === undefined ||
          state.connectRequested === undefined) {
        console.error('BAD STATE: ' + JSON.stringify(state));
      }
      return state.connectionState !==
          chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
    });
    this.observers_.forEach(o => o.onActiveNetworksChanged(activeNetworks));
  }

  onNetworkStateListChanged() {
    this.observers_.forEach(o => o.onNetworkStateListChanged());
  }

  onNetworkStateChanged(networkState) {
    // Calling onActiveNetworksChanged will trigger mojo checks on all
    // NetworkStateProperties. Ensure the networkState has name and guid field.
    if (networkState.name === undefined || networkState.guid === undefined) {
      return;
    }
    this.observers_.forEach(o => o.onNetworkStateChanged(networkState));
  }

  /** @param {string} userhash */
  onPoliciesApplied(userhash) {
    this.observers_.forEach(o => o.onPoliciesApplied(userhash));
  }

  onDeviceStateListChanged() {
    this.observers_.forEach(o => o.onDeviceStateListChanged());
  }

  onVpnProvidersChanged() {
    this.observers_.forEach(o => o.onVpnProvidersChanged());
  }

  onNetworkCertificatesChanged() {
    this.observers_.forEach(o => o.onNetworkCertificatesChanged());
  }

  // networkConfig methods

  /**
   * @param {!chromeos.networkConfig.mojom.CrosNetworkConfigObserverRemote}
   *     observer
   */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.NetworkStateProperties}>}
   */
  getNetworkState(guid) {
    return new Promise(resolve => {
      const result = this.networkStates_.find(state => {
        return state.guid === guid;
      });
      this.methodCalled('getNetworkState');
      resolve({result: result || null});
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType} connectionState
   * @param {!chromeos.networkConfig.mojom.FilterType} filterType
   * @return {boolean} Whether the connectionState type is not filtered out.
   */
  passFilter(connectionState, filterType) {
    switch (filterType) {
      case chromeos.networkConfig.mojom.FilterType.kActive:
        return connectionState !==
            chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
      case chromeos.networkConfig.mojom.FilterType.kVisible:
        return true;
      case chromeos.networkConfig.mojom.FilterType.kConfigured:
        return true;
      case chromeos.networkConfig.mojom.FilterType.kAll:
        return true;
    }
    assertNotReached('Failed to find filterType: ' + filterType.toString());
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkFilter} filter
   * @return {!Promise<{result:
   *     !Array<!chromeos.networkConfig.mojom.NetworkStateProperties>}>}
   */
  getNetworkStateList(filter) {
    return new Promise(resolve => {
      const networkType = filter.networkType;
      const filterType = filter.filter;
      const limit = filter.limit;
      let result;
      if (networkType === chromeos.networkConfig.mojom.NetworkType.kAll) {
        result = this.networkStates_.filter(
            state => this.passFilter(state.connectionState, filterType));
      } else {
        result = this.networkStates_.filter(
            state =>
                (state.type === networkType &&
                 this.passFilter(state.connectionState, filterType)));
      }

      if (limit !== chromeos.networkConfig.mojom.NO_LIMIT) {
        result = result.slice(0, limit);
      }
      this.methodCalled('getNetworkStateList');
      resolve({result: result});
    });
  }

  /**
   * @return {!Promise<{result:
   *     !Array<!chromeos.networkConfig.mojom.DeviceStateProperties>}>}
   */
  getDeviceStateList() {
    return new Promise(resolve => {
      const devices = [];
      this.deviceStates_.forEach((state, type) => {
        if (state.deviceState !==
            chromeos.networkConfig.mojom.DeviceStateType.kUninitialized) {
          devices.push(state);
        }
      });
      if (this.beforeGetDeviceStateList) {
        this.beforeGetDeviceStateList();
        this.beforeGetDeviceStateList = null;
      }
      this.methodCalled('getDeviceStateList');
      resolve({result: devices});
    });
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.ManagedProperties}>}
   */
  getManagedProperties(guid) {
    return new Promise(resolve => {
      let result = this.managedProperties_.get(guid);
      if (!result) {
        const foundState = this.networkStates_.find(state => {
          return state.guid === guid;
        });
        if (foundState) {
          result = OncMojo.getDefaultManagedProperties(
              foundState.type, foundState.guid, foundState.name);
        } else {
          console.error('GUID not found: ' + guid);
        }
      }
      this.methodCalled('getManagedProperties');
      resolve({result: result || null});
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.CellularSimState} cellularSimState
   * @return {!Promise<{success: boolean}>}
   */
  setCellularSimState(cellularSimState) {
    return new Promise(resolve => {
      const completeSetCellularSimState = (success) => {
        this.methodCalled('setCellularSimState');
        this.onDeviceStateListChanged();
        resolve({success: success});
      };

      // This is only called by cellular networks.
      const type = chromeos.networkConfig.mojom.NetworkType.kCellular;
      const deviceState = this.deviceStates_.get(type);
      let simLockStatus = deviceState.simLockStatus;
      const pin = this.testPin ? this.testPin : DEFAULT_CELLULAR_PIN;

      // If the correct pin is entered.
      if (cellularSimState.currentPinOrPuk === pin) {
        if (cellularSimState.newPin) {
          // Set new pin.
          this.testPin = cellularSimState.newPin;
          completeSetCellularSimState(/*success*/ true);
          return;
        }

        // toggling lock status.
        simLockStatus.lockEnabled = !simLockStatus.lockEnabled;
        deviceState.simLockStatus = simLockStatus;
        this.deviceStates_.set(type, deviceState);
        completeSetCellularSimState(/*success*/ true);
        return;
      }

      // Wrong pin entered.
      if (simLockStatus.retriesLeft > 1) {
        // If there is more than one retries left.
        simLockStatus.retriesLeft--;
        deviceState.simLockStatus = simLockStatus;
        this.deviceStates_.set(type, deviceState);
        completeSetCellularSimState(/*success*/ false);
        return;
      }

      // No retried left.
      simLockStatus = {lockEnabled: true, lockType: 'sim-puk', retriesLeft: 0};
      deviceState.simLockStatus = simLockStatus;
      this.deviceStates_.set(type, deviceState);
      completeSetCellularSimState(/*success*/ false);
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @param {boolean} enabled
   * @return {!Promise<{success: boolean}>}
   */
  setNetworkTypeEnabledState(type, enabled) {
    return new Promise(resolve => {
      const deviceState = this.deviceStates_.get(type);
      assert(!!deviceState, 'Unrecognized device type: ' + type);
      deviceState.deviceState = enabled ?
          chromeos.networkConfig.mojom.DeviceStateType.kEnabled :
          chromeos.networkConfig.mojom.DeviceStateType.kDisabled;
      this.methodCalled('setNetworkTypeEnabledState');
      this.onDeviceStateListChanged();
      resolve(true);
    });
  }

  /** @param {!chromeos.networkConfig.mojom.NetworkType } type */
  requestNetworkScan(type) {
    this.methodCalled('requestNetworkScan');
  }

  /**
   * @return {!Promise<{result: !chromeos.networkConfig.mojom.GlobalPolicy}>}
   */
  getGlobalPolicy() {
    return new Promise(resolve => {
      this.methodCalled('getGlobalPolicy');
      resolve({result: this.globalPolicy_});
    });
  }

  /** @param {!chromeos.networkConfig.mojom.GlobalPolicy} globalPolicy */
  setGlobalPolicy(globalPolicy) {
    this.globalPolicy_ = globalPolicy;
    this.onPoliciesApplied(/*userhash=*/ '');
  }

  /**
   * @return {!Promise<{
   *     result: !Array<!chromeos.networkConfig.mojom.VpnProvider>}>}
   */
  getVpnProviders() {
    return new Promise(resolve => {
      this.methodCalled('getVpnProviders');
      resolve({providers: this.vpnProviders_});
    });
  }

  /**
   * @return {!Promise<{result: !Array<string>}>}
   */
  getSupportedVpnTypes() {
    return new Promise(resolve => {
      this.methodCalled('getSupportedVpnTypes');
      resolve({
        vpnTypes: [
          'ikev2', 'l2tpipsec', 'openvpn', 'thirdpartyvpn', 'arcvpn',
          'wireguard'
        ]
      });
    });
  }

  /**
   * @return {!Promise<{
   *     serverCas: !Array<!chromeos.networkConfig.mojom.NetworkCertificate>,
   *     userCerts: !Array<!chromeos.networkConfig.mojom.NetworkCertificate>}>}
   */
  getNetworkCertificates() {
    return new Promise(resolve => {
      this.methodCalled('getNetworkCertificates');
      resolve({serverCas: this.serverCas_, userCerts: this.userCerts_});
    });
  }

  /**
   * @return {!Promise<{
   *      result: !chromeos.networkConfig.mojom.AlwaysOnVpnProperties}>}
   */
  getAlwaysOnVpn() {
    return new Promise(resolve => {
      this.methodCalled('getAlwaysOnVpn');
      resolve({properties: this.alwaysOnVpnProperties_});
    });
  }

  /**
   * @param {!chromeos.networkConfig.mojom.AlwaysOnVpnProperties} properties
   */
  setAlwaysOnVpn(properties) {
    this.alwaysOnVpnProperties_ = properties;
  }

  /**
   * @param {string} guid
   * @return {!Promise<!Array<!Object>>} traffic counters for network with guid
   */
  requestTrafficCounters(guid) {
    return new Promise(resolve => {
      this.methodCalled('requestTrafficCounters');
      resolve({trafficCounters: this.trafficCountersMap_.get(guid)});
    });
  }

  /**
   * @param {string} guid
   */
  resetTrafficCounters(guid) {
    const trafficCounters = this.trafficCountersMap_.get(guid);
    assert(!!trafficCounters, 'Network not found: ' + guid);
    trafficCounters.forEach(function(counter) {
      counter.rxBytes = 0;
      counter.txBytes = 0;
    });
    this.methodCalled('resetTrafficCounters');
  }

  /**
   * @param {string} guid
   * @param {boolean} autoReset
   * @param {?chromeos.networkConfig.mojom.UInt32Value} resetDay
   */
  setAutoResetValues_(guid, autoReset, resetDay) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    const managed = this.managedProperties_.get(guid);
    if (managed) {
      managed.trafficCounterProperties.autoReset = autoReset;
      managed.trafficCounterProperties.userSpecifiedResetDay =
          resetDay ? resetDay.value : 1;
    }
    this.onActiveNetworksChanged();
  }

  /**
   * @param {string} guid
   * @param {boolean} autoReset
   * @param {?chromeos.networkConfig.mojom.UInt32Value} resetDay
   */
  setTrafficCountersAutoReset(guid, autoReset, resetDay) {
    return new Promise(resolve => {
      this.methodCalled('setTrafficCountersAutoReset');
      this.setAutoResetValues_(guid, autoReset, resetDay);
      resolve(true);
    });
  }
}
