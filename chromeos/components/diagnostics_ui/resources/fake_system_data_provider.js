// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, CpuUsageObserver, ExternalPowerSource, MemoryUsage, MemoryUsageObserver, SystemInfo} from './diagnostics_types.js';
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

    this.registerMethods();
    this.registerObservables();
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
    return this.observe_(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated',
        (batteryChargeStatus) => {
          remote.onBatteryChargeStatusUpdated(
              /** @type {!BatteryChargeStatus} */ (batteryChargeStatus));
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
    return this.observe_(
        'BatteryHealthObserver_onBatteryHealthUpdated', (batteryHealth) => {
          remote.onBatteryHealthUpdated(
              /** @type {!BatteryHealth} */ (batteryHealth));
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
    return this.observe_('CpuUsageObserver_onCpuUsageUpdated', (cpuUsage) => {
      remote.onCpuUsageUpdated(
          /** @type {!CpuUsage} */ (cpuUsage));
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

  /*
   * Implements SystemDataProviderInterface.ObserveMemoryUsage.
   * @param {!MemoryUsageObserver} remote
   * @return {!Promise}
   */
  observeMemoryUsage(remote) {
    return this.observe_(
        'MemoryUsageObserver_onMemoryUsageUpdated', (memoryUsage) => {
          remote.onMemoryUsageUpdated(
              /** @type {!MemoryUsage} */ (memoryUsage));
        });
  }

  /**
   * Sets the values that will observed from ObserveCpuUsage.
   * @param {!Array<!MemoryUsage>} memoryUsageList
   */
  setFakeMemoryUsage(memoryUsageList) {
    this.observables_.setObservableData(
        'MemoryUsageObserver_onMemoryUsageUpdated', memoryUsageList);
  }

  /**
   * Make the observables fire automatically on various intervals.
   */
  startTriggerIntervals() {
    this.observables_.startTriggerOnInterval(
        'CpuUsageObserver_onCpuUsageUpdated', 1000);
    this.observables_.startTriggerOnInterval(
        'MemoryUsageObserver_onMemoryUsageUpdated', 5000);
    this.observables_.startTriggerOnInterval(
        'BatteryHealthObserver_onBatteryHealthUpdated', 30000);
    this.observables_.startTriggerOnInterval(
        'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated', 30000);
  }

  /**
   * Stop automatically triggering observables.
   */
  stopTriggerIntervals() {
    this.observables_.stopAllTriggerIntervals();
  }

  /**
   * Setup method resolvers.
   */
  registerMethods() {
    this.methods_.register('getSystemInfo');
    this.methods_.register('getBatteryInfo');
  }

  /**
   * Setup observables.
   */
  registerObservables() {
    this.observables_.register(
    'BatteryChargeStatusObserver_onBatteryChargeStatusUpdated');
    this.observables_.register('BatteryHealthObserver_onBatteryHealthUpdated');
    this.observables_.register('CpuUsageObserver_onCpuUsageUpdated');
    this.observables_.register('MemoryUsageObserver_onMemoryUsageUpdated');
  }

  /**
   * Disables all observers and resets provider to its initial state.
   */
  reset() {
    this.observables_.stopAllTriggerIntervals();

    this.methods_ = new FakeMethodResolver();
    this.observables_ = new FakeObservables();

    this.registerMethods();
    this.registerObservables();
  }

  /*
   * Sets up an observer for methodName.
   * @template T
   * @param {string} methodName
   * @param {!function(!T)} callback
   * @return {!Promise}
   * @private
   */
  observe_(methodName, callback) {
    return new Promise((resolve) => {
      this.observables_.observe(methodName, callback);
      this.observables_.trigger(methodName);
      resolve();
    });
  }
}
