// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of CrosNetworkConfig for testing.
 */

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {PromiseResolver} from 'chrome://resources/ash/common/promise_resolver.js';
import {AlwaysOnVpnMode, AlwaysOnVpnProperties, ApnProperties, ApnState, CellularSimState, ConfigProperties, CrosNetworkConfigInterface, CrosNetworkConfigObserverRemote, DeviceStateProperties, FilterType, GlobalPolicy, InhibitReason, ManagedProperties, NetworkCertificate, NetworkFilter, NetworkStateProperties, NO_LIMIT, StartConnectResult, TrafficCounter, UInt32Value, VpnProvider} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, DeviceStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

// Default cellular pin, used when locking/unlocking cellular profiles.
export const DEFAULT_CELLULAR_PIN = '1111';

/**
 * @implements {CrosNetworkConfigInterface}
 */
export class FakeNetworkConfig {
  constructor() {
    /** @private {!Map<string, !PromiseResolver>} */
    this.resolverMap_ = new Map();

    /**
     * @private {!Map<NetworkType,
     *     !DeviceStateProperties>}
     */
    this.deviceStates_ = new Map();

    /**
     * @private {!Array<!NetworkStateProperties>}
     */
    this.networkStates_ = [];

    /**
     * @private {!Map<string, !ManagedProperties>}
     */
    this.managedProperties_ = new Map();

    /**
     * @private {!ConfigProperties|undefined}
     */
    this.propertiesToSet_ = undefined;

    /** @private {!GlobalPolicy|undefined} */
    this.globalPolicy_ = undefined;

    /** @private {!Array<!NetworkCertificate>} */
    this.serverCas_ = [];

    /** @private {!Array<!NetworkCertificate>} */
    this.userCerts_ = [];

    /**
     * @private {!Array<!CrosNetworkConfigObserverRemote>}
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
     * @private {AlwaysOnVpnProperties}
     */
    this.alwaysOnVpnProperties_ = {
      mode: AlwaysOnVpnMode.kOff,
      serviceGuid: '',
    };

    /** @type {Function} */
    this.beforeGetDeviceStateList = null;

    /** @type {Function} */
    this.beforeGetManagedProperties = null;

    /** @private {!Array<VpnProvider>} */
    this.vpnProviders_ = [];

    /** @private {!Map<string, !Array<!Object>>} */
    this.trafficCountersMap_ = new Map();

    /** @private {!number} */
    this.apnIdCounter_ = 0;

    this.resetForTest();
  }

  /**
   * @param {NetworkType} type
   * @return {DeviceStateProperties}
   * @private
   */
  addDeviceState_(type) {
    assert(type !== undefined);
    const deviceState =
        /** @type {!DeviceStateProperties} */ ({
          type: type,
          deviceState: DeviceStateType.kUninitialized,
          inhibitReason: InhibitReason.kNotInhibited,
        });
    this.deviceStates_.set(type, deviceState);
    return deviceState;
  }

  resetForTest() {
    this.deviceStates_ = new Map();
    this.addDeviceState_(NetworkType.kEthernet).deviceState =
        DeviceStateType.kEnabled;
    this.addDeviceState_(NetworkType.kWiFi);
    this.addDeviceState_(NetworkType.kCellular);
    this.addDeviceState_(NetworkType.kTether);
    this.addDeviceState_(NetworkType.kVPN);

    this.globalPolicy_ =
        /** @type {!GlobalPolicy} */ ({
          allowApnModification: true,
          allow_cellular_sim_lock: true,
          allow_only_policy_cellular_networks: false,
          allow_only_policy_networks_to_autoconnect: false,
          allow_only_policy_wifi_networks_to_connect: false,
          allow_only_policy_wifi_networks_to_connect_if_available: false,
          dns_queries_monitored: false,
          report_xdr_events_enabled: false,
          blocked_hex_ssids: [],
        });

    const eth0 = OncMojo.getDefaultNetworkState(NetworkType.kEthernet, 'eth0');
    this.networkStates_ = [eth0];

    this.managedProperties_ = new Map();
    this.propertiesToSet_ = undefined;

    this.vpnProviders_ = [];

    this.serverCas_ = [];
    this.userCerts_ = [];

    ['getNetworkState', 'getNetworkStateList', 'getDeviceStateList',
     'getManagedProperties', 'setNetworkTypeEnabledState', 'requestNetworkScan',
     'getGlobalPolicy', 'getVpnProviders', 'getNetworkCertificates',
     'setProperties', 'setCellularSimState', 'selectCellularMobileNetwork',
     'startConnect', 'startDisconnect', 'configureNetwork', 'forgetNetwork',
     'getAlwaysOnVpn', 'getSupportedVpnTypes', 'requestTrafficCounters',
     'resetTrafficCounters', 'setTrafficCountersResetDay', 'removeCustomApn',
     'createCustomApn', 'createExclusivelyEnabledCustomApn', 'modifyCustomApn']
        .forEach((methodName) => {
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
   * @param {!Array<!NetworkStateProperties>}
   *     networks
   */
  addNetworksForTest(networks) {
    this.networkStates_ = this.networkStates_.concat(networks);
    this.onNetworkStateListChanged();
  }

  /**
   * @param {!NetworkStateProperties} network
   */
  removeNetworkForTest(network) {
    this.networkStates_ = this.networkStates_.filter((state) => {
      return state.guid !== network.guid;
    });
    this.onNetworkStateListChanged();
  }

  /**
   * @param {!ManagedProperties} network
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
   * @param {ConnectionStateType} state
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
   * @param {boolean} visible
   */
  setWifiNetworkVisibleForTest(guid, visible) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    assert(
        network.type === NetworkType.kWiFi,
        'Network visible can only be set on WiFi type');
    network.typeState.wifi.visible = visible;

    this.onNetworkStateChanged(network);
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
   * @param {?Time} lastResetTime last reset
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
   * @return {!Promise<{ result: !StartConnectResult, message: !string,}>}
   */
  startConnect(guid) {
    return new Promise(resolve => {
      this.methodCalled('startConnect');
      resolve({result: StartConnectResult.kCanceled, message: ''});
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
      network.connectionState = ConnectionStateType.kNotConnected;
      this.methodCalled('startDisconnect');
      resolve({success: true});
    });
  }

  /**
   * @param {ConfigProperties} properties
   * @param {boolean} shared
   * @return {!Promise<{guid: string, errorMessage: string}>}
   */
  configureNetwork(properties, shared) {
    return new Promise(resolve => {
      this.propertiesToSet_ =
          /** @type(!ConfigProperties)*/
          (Object.assign({}, properties));
      this.methodCalled('configureNetwork');
      resolve({guid: 'test_guid', errorMessage: ''});
    });
  }

  /**
   * @param {!string} guid
   * @return {!Promise<{success: !boolean}>}
   */
  forgetNetwork(guid) {
    return new Promise(resolve => {
      this.methodCalled('forgetNetwork');
      resolve({success: true});
    });
  }

  /**
   * @param {string} guid
   * @param {ConfigProperties} properties
   * @return {!Promise<{success: boolean, errorMessage: string}>}
   */
  setProperties(guid, properties) {
    return new Promise(resolve => {
      this.propertiesToSet_ =
          /** @type(!ConfigProperties)*/ (Object.assign({}, properties));
      this.methodCalled('setProperties');
      resolve({success: true, errorMessage: ''});
    });
  }

  /**
   * @param {DeviceStateProperties} deviceState
   */
  setDeviceStateForTest(deviceState) {
    assert(deviceState.type !== undefined);
    this.deviceStates_.set(deviceState.type, deviceState);
    this.onDeviceStateListChanged();
  }

  /**
   * @param {NetworkType} type
   * @return {?DeviceStateProperties}
   */
  getDeviceStateForTest(type) {
    return this.deviceStates_.get(type) || null;
  }

  /** @return {!ConfigProperties|undefined} */
  getPropertiesToSetForTest() {
    return this.propertiesToSet_;
  }

  /** @param {!Array<!VpnProvider>} providers */
  setVpnProvidersForTest(providers) {
    this.vpnProviders_ = providers;
    this.onVpnProvidersChanged();
  }

  /**
   * @param {!Array<!NetworkCertificate>} serverCas
   * @param {!Array<!NetworkCertificate>} userCerts
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
      return state.connectionState !== ConnectionStateType.kNotConnected;
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
   * @param {!CrosNetworkConfigObserverRemote}
   *     observer
   */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  /**
   * @param {string} guid
   * @return {!Promise<{result:
   *     !NetworkStateProperties}>}
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
   * @param {!ConnectionStateType} connectionState
   * @param {!FilterType} filterType
   * @return {boolean} Whether the connectionState type is not filtered out.
   */
  passFilter(connectionState, filterType) {
    switch (filterType) {
      case FilterType.kActive:
        return connectionState !== ConnectionStateType.kNotConnected;
      case FilterType.kVisible:
        return true;
      case FilterType.kConfigured:
        return true;
      case FilterType.kAll:
        return true;
    }
    assertNotReached('Failed to find filterType: ' + filterType.toString());
  }

  /**
   * @param {!NetworkFilter} filter
   * @return {!Promise<{result:
   *     !Array<!NetworkStateProperties>}>}
   */
  getNetworkStateList(filter) {
    return new Promise(resolve => {
      const networkType = filter.networkType;
      const filterType = filter.filter;
      const limit = filter.limit;
      let result;
      if (networkType === NetworkType.kAll) {
        result = this.networkStates_.filter(
            state => this.passFilter(state.connectionState, filterType));
      } else {
        result = this.networkStates_.filter(
            state =>
                (state.type === networkType &&
                 this.passFilter(state.connectionState, filterType)));
      }

      if (limit !== NO_LIMIT) {
        result = result.slice(0, limit);
      }

      this.methodCalled('getNetworkStateList');
      resolve({result: result});
    });
  }

  /**
   * @return {!Promise<{result:
   *     !Array<!DeviceStateProperties>}>}
   */
  getDeviceStateList() {
    return new Promise(resolve => {
      const devices = [];
      this.deviceStates_.forEach((state, type) => {
        if (state.deviceState !== DeviceStateType.kUninitialized) {
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
   *     !ManagedProperties}>}
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
          console.warn('GUID not found: ' + guid);
        }
      }
      if (this.beforeGetManagedProperties) {
        this.beforeGetManagedProperties();
        this.beforeGetManagedProperties = null;
      }
      this.methodCalled('getManagedProperties');
      resolve({result: result || null});
    });
  }

  /**
   * @param {!NetworkType} type
   * @return {boolean}
   */
  getIsDeviceScanning(type) {
    const deviceState = this.deviceStates_.get(type);
    assert(!!deviceState);
    return deviceState.scanning;
  }

  /**
   * @param {!CellularSimState} cellularSimState
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
      const type = NetworkType.kCellular;
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
   * @param { !string } guid
   * @param { !string } networkId
   * @return {!Promise<{success: !boolean}>}
   */
  selectCellularMobileNetwork(guid, networkId) {
    return new Promise(resolve => {
      this.methodCalled('selectCellularMobileNetwork');
      resolve({success: false});
    });
  }

  /**
   * @param {!NetworkType} type
   * @param {boolean} enabled
   * @return {!Promise<{success: boolean}>}
   */
  setNetworkTypeEnabledState(type, enabled) {
    return new Promise(resolve => {
      const deviceState = this.deviceStates_.get(type);
      assert(!!deviceState, 'Unrecognized device type: ' + type);
      deviceState.deviceState =
          enabled ? DeviceStateType.kEnabled : DeviceStateType.kDisabled;
      this.methodCalled('setNetworkTypeEnabledState');
      this.onDeviceStateListChanged();
      resolve(true);
    });
  }

  /** @param {!NetworkType } type */
  requestNetworkScan(type) {
    this.deviceStates_.get(type).scanning = true;
    this.onDeviceStateListChanged();

    this.methodCalled('requestNetworkScan');
  }

  /**
   * @return {!Promise<{result: !GlobalPolicy}>}
   */
  getGlobalPolicy() {
    return new Promise(resolve => {
      this.methodCalled('getGlobalPolicy');
      resolve({result: this.globalPolicy_});
    });
  }

  /** @param {!GlobalPolicy|undefined} globalPolicy */
  setGlobalPolicy(globalPolicy) {
    this.globalPolicy_ = globalPolicy;
    this.onPoliciesApplied(/*userhash=*/ '');
  }

  /**
   * @return {!Promise<{
   *     providers: !Array<!VpnProvider>}>}
   */
  getVpnProviders() {
    return new Promise(resolve => {
      this.methodCalled('getVpnProviders');
      resolve({providers: this.vpnProviders_});
    });
  }

  /**
   * @param { !Array<!VpnProvider> } providers
   */
  setVpnProviders(providers) {
    this.vpnProviders_ = providers;
    this.onVpnProvidersChanged();
  }

  /**
   * @return {!Promise<{vpnTypes: !Array<string>}>}
   */
  getSupportedVpnTypes() {
    return new Promise(resolve => {
      this.methodCalled('getSupportedVpnTypes');
      resolve({
        vpnTypes: [
          'ikev2',
          'l2tpipsec',
          'openvpn',
          'thirdpartyvpn',
          'arcvpn',
          'wireguard',
        ],
      });
    });
  }

  /**
   * @return {!Promise<{
   *     serverCas: !Array<!NetworkCertificate>,
   *     userCerts: !Array<!NetworkCertificate>}>}
   */
  getNetworkCertificates() {
    return new Promise(resolve => {
      this.methodCalled('getNetworkCertificates');
      resolve({serverCas: this.serverCas_, userCerts: this.userCerts_});
    });
  }

  /**
   * @return {!Promise<{
   *      properties: !AlwaysOnVpnProperties}>}
   */
  getAlwaysOnVpn() {
    return new Promise(resolve => {
      this.methodCalled('getAlwaysOnVpn');
      resolve({properties: this.alwaysOnVpnProperties_});
    });
  }

  /**
   * @param {!AlwaysOnVpnProperties} properties
   */
  setAlwaysOnVpn(properties) {
    this.alwaysOnVpnProperties_ = properties;
  }

  /**
   * @param {string} guid
   * @return {!Promise<{trafficCounters: !Array<!TrafficCounter>}>} traffic
   *     counters for network with guid
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
   * @param {?UInt32Value} resetDay
   */
  setResetDay_(guid, resetDay) {
    const network = this.networkStates_.find(state => {
      return state.guid === guid;
    });
    assert(!!network, 'Network not found: ' + guid);
    const managed = this.managedProperties_.get(guid);
    if (managed) {
      managed.trafficCounterProperties.userSpecifiedResetDay =
          resetDay ? resetDay.value : 1;
    }
    this.onActiveNetworksChanged();
  }

  /**
   * @param {string} guid
   * @param {?UInt32Value} resetDay
   */
  setTrafficCountersResetDay(guid, resetDay) {
    return new Promise(resolve => {
      this.methodCalled('setTrafficCountersResetDay');
      this.setResetDay_(guid, resetDay);
      resolve(true);
    });
  }

  /**
   * @param {!string} guid
   * @param {!ApnProperties} apn
   */
  createCustomApn(guid, apn) {
    return new Promise(resolve => {
      const properties = this.managedProperties_.get(guid);
      assert(properties);
      apn.id = `${this.apnIdCounter_++}`;
      if (!properties.typeProperties.cellular.customApnList) {
        properties.typeProperties.cellular.customApnList = [];
      }
      properties.typeProperties.cellular.customApnList.unshift(apn);
      this.methodCalled('createCustomApn');
      resolve(true);
    });
  }

  /**
   * @param {!string} guid
   * @param {!ApnProperties} apn
   */
  createExclusivelyEnabledCustomApn(guid, apn) {
    return new Promise(resolve => {
      const properties = this.managedProperties_.get(guid);
      assert(properties);
      apn.id = `${this.apnIdCounter_++}`;
      if (!properties.typeProperties.cellular.customApnList) {
        properties.typeProperties.cellular.customApnList = [];
      }
      properties.typeProperties.cellular.customApnList.forEach(customApn => {
        customApn.state = ApnState.kDisabled;
      });
      apn.state = ApnState.kEnabled;
      properties.typeProperties.cellular.customApnList.unshift(apn);
      this.methodCalled('createExclusivelyEnabledCustomApn');
      resolve(true);
    });
  }

  /**
   * @param {string} guid
   * @param {string} apnId
   */
  removeCustomApn(guid, apnId) {
    assert(guid);
    assert(apnId);
    const managed = this.managedProperties_.get(guid);
    if (!!managed && !!managed.typeProperties &&
        !!managed.typeProperties.cellular &&
        Array.isArray(managed.typeProperties.cellular.customApnList)) {
      managed.typeProperties.cellular.customApnList =
          managed.typeProperties.cellular.customApnList.filter(
              apn => apn.id !== apnId);
    }
    this.methodCalled('removeCustomApn');
  }

  /**
   * @param {string} guid
   * @param {ApnProperties} apn
   */
  modifyCustomApn(guid, apn) {
    assert(guid);
    assert(apn);
    const managed = this.managedProperties_.get(guid);
    if (!!managed && !!managed.typeProperties &&
        !!managed.typeProperties.cellular &&
        Array.isArray(managed.typeProperties.cellular.customApnList)) {
      const index = managed.typeProperties.cellular.customApnList.findIndex(
          currentApn => currentApn.id === apn.id);
      if (index !== -1) {
        managed.typeProperties.cellular.customApnList[index] = apn;
      }
    }
    this.methodCalled('modifyCustomApn');
  }
}
