// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {CrostiniBrowserProxy} */
export class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'exportCrostiniContainer',
      'importCrostiniContainer',
      'requestCrostiniContainerUpgradeView',
      'requestCrostiniUpgraderDialogStatus',
      'requestCrostiniContainerUpgradeAvailable',
      'getCrostiniDiskInfo',
      'resizeCrostiniDisk',
      'addCrostiniPortForward',
      'removeCrostiniPortForward',
      'removeAllCrostiniPortForwards',
      'activateCrostiniPortForward',
      'deactivateCrostiniPortForward',
      'getCrostiniActivePorts',
      'getCrostiniActiveNetworkInfo',
      'checkCrostiniIsRunning',
      'shutdownCrostini',
      'setCrostiniMicSharingEnabled',
      'getCrostiniMicSharingEnabled',
      'requestCrostiniInstallerStatus',
      'requestArcAdbSideloadStatus',
      'getCanChangeArcAdbSideloading',
      'createContainer',
      'deleteContainer',
      'requestContainerInfo',
      'setContainerBadgeColor',
      'stopContainer',
      'requestCrostiniExportImportOperationStatus',
      'openContainerFileSelector',
      'requestSharedVmDevices',
      'isVmDeviceShared',
      'setVmDeviceShared',
      'requestBruschettaInstallerView',
      'requestBruschettaUninstallerView',
    ]);
    this.crostiniMicSharingEnabled = false;
    this.crostiniIsRunning = true;
    this.methodCalls_ = {};
    this.portOperationSuccess = true;
    this.containerInfo = [];
    this.selectedContainerFileName = '';
    this.sharedVmDevices = [];
  }

  getNewPromiseFor(name) {
    if (name in this.methodCalls_) {
      return new Promise((resolve, reject) => {
        this.methodCalls_[name].push({name, resolve, reject});
      });
    } else {
      return new Promise((resolve, reject) => {
        this.methodCalls_[name] = [{name, resolve, reject}];
      });
    }
  }

  async resolvePromises(name, ...args) {
    for (const o of this.methodCalls_[name]) {
      await o.resolve(...args);
    }
    this.methodCalls_[name] = [];
  }

  async rejectPromises(name, ...args) {
    for (const o of this.methodCalls_[name]) {
      await o.reject(...args);
    }
    this.methodCalls_[name] = [];
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
  }

  /**override */
  requestArcAdbSideloadStatus() {
    this.methodCalled('requestArcAdbSideloadStatus');
  }

  /** override */
  getCanChangeArcAdbSideloading() {
    this.methodCalled('getCanChangeArcAdbSideloading');
  }

  /** @override */
  requestCrostiniInstallerStatus() {
    this.methodCalled('requestCrostiniInstallerStatus');
    webUIListenerCallback('crostini-installer-status-changed', false);
  }

  /** @override */
  requestCrostiniExportImportOperationStatus() {
    this.methodCalled('requestCrostiniExportImportOperationStatus');
    webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);
  }

  /** override */
  exportCrostiniContainer(containerId) {
    this.methodCalled('exportCrostiniContainer', containerId);
  }

  /** override */
  importCrostiniContainer(containerId) {
    this.methodCalled('importCrostiniContainer', containerId);
  }

  /** @override */
  requestCrostiniContainerUpgradeView() {
    this.methodCalled('requestCrostiniContainerUpgradeView');
  }

  /** @override */
  requestCrostiniUpgraderDialogStatus() {
    webUIListenerCallback('crostini-upgrader-status-changed', false);
  }

  /** @override */
  requestCrostiniContainerUpgradeAvailable() {
    webUIListenerCallback('crostini-container-upgrade-available-changed', true);
  }

  /** @override */
  addCrostiniPortForward(containerId, portNumber, protocolIndex, label) {
    this.methodCalled(
        'addCrostiniPortForward', containerId, portNumber, protocolIndex,
        label);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  removeCrostiniPortForward(containerId, portNumber, protocolIndex) {
    this.methodCalled(
        'removeCrostiniPortForward', containerId, portNumber, protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  activateCrostiniPortForward(containerId, portNumber, protocolIndex) {
    this.methodCalled(
        'activateCrostiniPortForward', containerId, portNumber, protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  deactivateCrostiniPortForward(containerId, portNumber, protocolIndex) {
    this.methodCalled(
        'deactivateCrostiniPortForward', containerId, portNumber,
        protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  /** @override */
  removeAllCrostiniPortForwards(containerId) {
    this.methodCalled('removeAllCrostiniPortForwards', containerId);
  }

  /** @override */
  getCrostiniActivePorts() {
    this.methodCalled('getCrostiniActivePorts');
    return Promise.resolve([]);
  }

  getCrostiniActiveNetworkInfo() {
    this.methodCalled('getCrostiniActiveNetworkInfo');
    return Promise.resolve([]);
  }

  /** @override */
  getCrostiniDiskInfo(vmName, requestFullInfo) {
    this.methodCalled('getCrostiniDiskInfo', vmName, requestFullInfo);
    return this.getNewPromiseFor('getCrostiniDiskInfo');
  }

  /** @override */
  resizeCrostiniDisk(vmName, newSizeBytes) {
    this.methodCalled('resizeCrostiniDisk', vmName, newSizeBytes);
    return this.getNewPromiseFor('resizeCrostiniDisk');
  }

  /** @override */
  checkCrostiniIsRunning() {
    this.methodCalled('checkCrostiniIsRunning');
    return Promise.resolve(this.crostiniIsRunning);
  }

  /** @override */
  shutdownCrostini() {
    this.methodCalled('shutdownCrostini');
    this.crostiniIsRunning = false;
  }

  /** @override */
  setCrostiniMicSharingEnabled(enabled) {
    this.methodCalled('setCrostiniMicSharingEnabled');
    this.crostiniMicSharingEnabled = enabled;
  }

  /** @override */
  getCrostiniMicSharingEnabled() {
    this.methodCalled('getCrostiniMicSharingEnabled');
    return Promise.resolve(this.CrostiniMicSharingEnabled);
  }

  /** @override */
  createContainer(containerId, imageServer, imageAlias, containerFile) {
    this.methodCalled(
        'createContainer', containerId, imageServer, imageAlias, containerFile);
  }

  /** @override */
  deleteContainer(containerId) {
    this.methodCalled('deleteContainer');
  }

  /** @override */
  requestContainerInfo() {
    this.methodCalled('requestContainerInfo');
    webUIListenerCallback('crostini-container-info', this.containerInfo);
  }

  /** @override */
  setContainerBadgeColor(containerId, badge_color) {
    this.methodCalled('setContainerBadgeColor');
  }

  /** @override */
  stopContainer(containerId) {
    this.methodCalled('stopContainer');
  }

  /** @override */
  openContainerFileSelector() {
    this.methodCalled('openContainerFileSelector');
    return Promise.resolve(this.selectedContainerFileName);
  }

  /** @override */
  requestSharedVmDevices() {
    this.methodCalled('requestSharedVmDevices');
    webUIListenerCallback('crostini-shared-vmdevices', this.sharedVmDevices);
  }

  /** @override */
  isVmDeviceShared(id, device) {
    this.methodCalled('isVmDeviceShared', id, device);
    return this.getNewPromiseFor('isVmDeviceShared');
  }

  /** @override */
  setVmDeviceShared(id, device, shared) {
    this.methodCalled('setVmDeviceShared', id, device, shared);
    return this.getNewPromiseFor('setVmDeviceShared');
  }

  /** @override */
  requestBruschettaInstallerView() {
    this.methodCalled('requestBruschettaInstallerView');
  }

  /** @override */
  requestBruschettaUninstallerView() {
    this.methodCalled('requestBruschettaUninstallerView');
  }
}
