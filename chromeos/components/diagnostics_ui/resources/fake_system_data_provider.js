// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, CpuUsageObserver, ExternalPowerSource, SystemInfo} from './diagnostics_types.js';
import {FakeMethodResolver} from './fake_method_resolver.js';
import {FakeObservables} from './fake_observables.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemDataProvider mojo interface.
 */

export class FakeSystemDataProvider {
  constructor() {
    /** @private {!FakeMethodResolver} */
    this.methods_ = new FakeMethodResolver();

    /** @private {!FakeObservables} */
    this.observables_ = new FakeObservables();

    // Setup method resolvers.
    this.methods_.register('getSystemInfo');
    this.methods_.register('getBatteryInfo');

    // Setup observables.
    this.observables_.register(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
    this.observables_.register('BatteryHealthObserver_onBatteryHealthUpdated');
    this.observables_.register('CpuUsageObserver_onCpuUsageUpdated');
  }

  /**
   * @return {!Promise<!SystemInfo>}
   */
  getSystemInfo() {
    return this.methods_.resolveMethod('getSystemInfo');
  }

  /**
   * Sets the value that will be returned when calling getSystemInfo().
   * @param {!SystemInfo} systemInfo
   */
  setFakeSystemInfo(systemInfo) {
    this.methods_.setResult('getSystemInfo', systemInfo);
  }

  /**
   * Implements SystemDataProviderInterface.GetBatteryInfo.
   * @return {!Promise<!BatteryInfo>}
   */
  getBatteryInfo() {
    return this.methods_.resolveMethod('getBatteryInfo');
  }

  /**
   * Sets the value that will be returned when calling getBatteryInfo().
   * @param {!BatteryInfo} batteryInfo
   */
  setFakeBatteryInfo(batteryInfo) {
    this.methods_.setResult('getBatteryInfo', batteryInfo);
  }

  /*
   * Implements SystemDataProviderInterface.ObserveBatteryChargeStatus.
   * @param {!BatteryChargeStatusObserver} remote
   * @return {!Promise}
   */
  observeBatteryChargeStatus(remote) {
    return new Promise((resolve) => {
      this.observables_.observe(
          'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
          (batteryChargeStatus) => {
            remote.onBatteryChargeStatusUpdated(
                /** @type {!BatteryChargeStatus} */ (batteryChargeStatus));
          });

      this.observables_.trigger(
          'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
      resolve();
    });
  }

  /**
   * Sets the values that will observed from observeBatteryChargeStatus.
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatusList
   */
  setFakeBatteryChargeStatus(batteryChargeStatusList) {
    this.observables_.setObservableData(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        batteryChargeStatusList);
  }

  /*
   * Implements SystemDataProviderInterface.ObserveBatteryHealth.
   * @param {!BatteryHealthObserver} remote
   * @return {!Promise}
   */
  observeBatteryHealth(remote) {
    return new Promise((resolve) => {
      this.observables_.observe(
          'BatteryHealthObserver_onBatteryHealthUpdated', (batteryHealth) => {
            remote.onBatteryHealthUpdated(
                /** @type {!BatteryHealth} */ (batteryHealth));
          });

      this.observables_.trigger('BatteryHealthObserver_onBatteryHealthUpdated');
      resolve();
    });
  }

  /**
   * Sets the values that will observed from observeBatteryHealth.
   * @param {!Array<!BatteryHealth>} batteryHealthList
   */
  setFakeBatteryHealth(batteryHealthList) {
    this.observables_.setObservableData(
        'BatteryHealthObserver_onBatteryHealthUpdated', batteryHealthList);
  }

  /*
   * Implements SystemDataProviderInterface.ObserveCpuUsage.
   * @param {!CpuUsageObserver} remote
   * @return {!Promise}
   */
  observeCpuUsage(remote) {
    return new Promise((resolve) => {
      this.observables_.observe(
          'CpuUsageObserver_onCpuUsageUpdated', (cpuUsage) => {
            remote.onCpuUsageUpdated(
                /** @type {!CpuUsage} */ (cpuUsage));
          });

      this.observables_.trigger('CpuUsageObserver_onCpuUsageUpdated');
      resolve();
    });
  }

  /**
   * Sets the values that will observed from observeCpuUsage.
   * @param {!Array<!CpuUsage>} cpuUsageList
   */
  setFakeCpuUsage(cpuUsageList) {
    this.observables_.setObservableData(
        'CpuUsageObserver_onCpuUsageUpdated', cpuUsageList);
  }
}
