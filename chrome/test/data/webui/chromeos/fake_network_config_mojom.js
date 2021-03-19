// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of CrosNetworkConfig for testing.
 */

 // clang-format off
 // #import {assert} from 'chrome://resources/js/assert.m.js';
 // #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
 // #import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
 // clang-format on

// Default cellular pin, used when locking/unlocking cellular profiles.
/* #export */ const DEFAULT_CELLULAR_PIN = '1111';

// TODO(stevenjb): Include cros_network_config.mojom.js and extend
// CrosNetworkConfigInterface
/* #export */ class FakeNetworkConfig {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @type {!Map<chromeos.networkConfig.mojom.NetworkType,
     *     !chromeos.networkConfig.mojom.DeviceStateProperties>}
     */
    this.deviceStates_ = new Map();

    /** @type {!Array<!chromeos.networkConfig.mojom.NetworkStateProperties>} */
    this.networkStates_ = [];

    /** @type {!Map<string, !chromeos.networkConfig.mojom.ManagedProperties>} */
    this.managedProperties_ = new Map();

    /** @type {!chromeos.networkConfig.mojom.GlobalPolicy|undefined} */
    this.globalPolicy_ = undefined;

    /** @type {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} */
    this.serverCas_ = [];

    /** @type {!Array<!chromeos.networkConfig.mojom.NetworkCertificate>} */
    this.userCerts_ = [];

    /**
     * @type {!Array<!chromeos.networkConfig.mojom.CrosNetworkConfigObserver>
     */
    this.observers_ = [];

    /**
     * When updating or changing cellular pin, |testPin| is used to store
     * the updated pin, if not set DEFAULT_CELLULAR_PIN is used to check pin
     * value in |setCellularSimState()|
     * @type {string}
     */
    this.testPin = '';

    this.resetForTest();
  }

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {chromeos.networkConfig.mojom.DeviceStateProperties}
   * @private
   */
  addDeviceState_(type) {
    assert(type !== undefined);
    const deviceState = {
      type: type,
      deviceState: chromeos.networkConfig.mojom.DeviceStateType.kUninitialized,
      inhibitReason: chromeos.networkConfig.mojom.InhibitReason.kNotInhibited
    };
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

    this.globalPolicy_ = {
      allow_only_policy_networks_to_autoconnect: false,
      allow_only_policy_networks_to_connect: false,
      allow_only_policy_networks_to_connect_if_available: false,
      blocked_hex_ssids: [],
    };

    const eth0 =
        OncMojo.getDefaultNetworkState(mojom.NetworkType.kEthernet, 'eth0');
    this.networkStates_ = [eth0];

    this.managedProperties_ = new Map();

    this.vpnProviders_ = [];

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
     'configureNetwork',
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
    let method = this.resolverMap_.get(methodName);
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
   *     network
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
   * @param {!chromeos.networkConfig.mojom.ManagedProperties>} network
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
    } else {
      this.networkStates_.push(networkState);
    }
  }

  /**
   * @param {string} guid
   * @param {chromeos.networkConfig.mojom.ConnectionStateType} state
   */
  setNetworkConnectionStateForTest(guid, state) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assertTrue(!!network, 'Network not found: ' + guid);
    network.connectionState = state;

    const managed = this.managedProperties_.get(guid);
    if (managed) {
      managed.connectionState = state;
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
   * @param {string} type
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
   * @param {!chromeos.networkConfig.mojom.CrosNetworkConfigObserverProxy }
   *     observer
   */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !chromeos.networkConfig.mojom.NetworkStateProperties>>}
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
   * @param {!chromeos.networkConfig.mojom.NetworkFilter} filter
   * @return {!Promise<{result:
   *     !Array<!chromeos.networkConfig.mojom.NetworkStateProperties>}>}
   */
  getNetworkStateList(filter) {
    return new Promise(resolve => {
      const type = filter.networkType;
      let result;
      if (type === chromeos.networkConfig.mojom.NetworkType.kAll) {
        result = this.networkStates_.slice();
      } else {
        result = this.networkStates_.filter(state => state.type === type);
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
      let deviceState = this.deviceStates_.get(type);
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
}
