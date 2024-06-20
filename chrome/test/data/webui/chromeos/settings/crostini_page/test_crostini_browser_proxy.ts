// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContainerInfo, CrostiniBrowserProxy, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, GuestId, ShareableDevices} from 'chrome://os-settings/lazy_load.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export interface SharedVmDevices {
  id: GuestId;
  vmDevices: ShareableDevices;
}

export class TestCrostiniBrowserProxy extends TestBrowserProxy implements
    CrostiniBrowserProxy {
  crostiniMicSharingEnabled: boolean;
  bruschettaIsRunning: boolean;
  crostiniIsRunning: boolean;
  methodCalls: any;
  portOperationSuccess: boolean;
  containerInfo: ContainerInfo[];
  selectedContainerFileName: string;
  sharedVmDevices: SharedVmDevices[];

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
      'checkBruschettaIsRunning',
      'checkCrostiniIsRunning',
      'shutdownBruschetta',
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
      'enableArcAdbSideload',
      'disableArcAdbSideload',
      'checkCrostiniMicSharingStatus',
    ]);
    this.crostiniMicSharingEnabled = false;
    this.bruschettaIsRunning = true;
    this.crostiniIsRunning = true;
    this.methodCalls = {};
    this.portOperationSuccess = true;
    this.containerInfo = [];
    this.selectedContainerFileName = '';
    this.sharedVmDevices = [];
  }

  getNewPromiseFor(name: string): Promise<any> {
    if (name in this.methodCalls) {
      return new Promise((resolve, reject) => {
        this.methodCalls[name].push({name, resolve, reject});
      });
    }
    return new Promise((resolve, reject) => {
      this.methodCalls[name] = [{name, resolve, reject}];
    });
  }

  async resolvePromises(name: string, ...args: any): Promise<void> {
    for (const o of this.methodCalls[name]) {
      await o.resolve(...args);
    }
    this.methodCalls[name] = [];
  }

  async rejectPromises(name: string, ...args: any): Promise<void> {
    for (const o of this.methodCalls[name]) {
      await o.reject(...args);
    }
    this.methodCalls[name] = [];
  }

  requestCrostiniInstallerView(): void {
    this.methodCalled('requestCrostiniInstallerView');
  }

  requestRemoveCrostini(): void {
    this.methodCalled('requestRemoveCrostini');
  }

  requestArcAdbSideloadStatus(): void {
    this.methodCalled('requestArcAdbSideloadStatus');
  }

  getCanChangeArcAdbSideloading(): void {
    this.methodCalled('getCanChangeArcAdbSideloading');
  }

  requestCrostiniInstallerStatus(): void {
    this.methodCalled('requestCrostiniInstallerStatus');
    webUIListenerCallback('crostini-installer-status-changed', false);
  }

  requestCrostiniExportImportOperationStatus(): void {
    this.methodCalled('requestCrostiniExportImportOperationStatus');
    webUIListenerCallback(
        'crostini-export-import-operation-status-changed', false);
  }

  exportCrostiniContainer(containerId: GuestId): void {
    this.methodCalled('exportCrostiniContainer', containerId);
  }

  importCrostiniContainer(containerId: GuestId): void {
    this.methodCalled('importCrostiniContainer', containerId);
  }

  requestCrostiniContainerUpgradeView(): void {
    this.methodCalled('requestCrostiniContainerUpgradeView');
  }

  requestCrostiniUpgraderDialogStatus(): void {
    webUIListenerCallback('crostini-upgrader-status-changed', false);
  }

  requestCrostiniContainerUpgradeAvailable(): void {
    webUIListenerCallback('crostini-container-upgrade-available-changed', true);
  }

  addCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocolIndex: CrostiniPortProtocol, label: string): Promise<boolean> {
    this.methodCalled(
        'addCrostiniPortForward', containerId, portNumber, protocolIndex,
        label);
    return Promise.resolve(this.portOperationSuccess);
  }

  removeCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocolIndex: CrostiniPortProtocol): Promise<boolean> {
    this.methodCalled(
        'removeCrostiniPortForward', containerId, portNumber, protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  activateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocolIndex: CrostiniPortProtocol): Promise<boolean> {
    this.methodCalled(
        'activateCrostiniPortForward', containerId, portNumber, protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  deactivateCrostiniPortForward(
      containerId: GuestId, portNumber: number,
      protocolIndex: CrostiniPortProtocol): Promise<boolean> {
    this.methodCalled(
        'deactivateCrostiniPortForward', containerId, portNumber,
        protocolIndex);
    return Promise.resolve(this.portOperationSuccess);
  }

  removeAllCrostiniPortForwards(containerId: GuestId): void {
    this.methodCalled('removeAllCrostiniPortForwards', containerId);
  }

  getCrostiniActivePorts(): Promise<CrostiniPortActiveSetting[]> {
    this.methodCalled('getCrostiniActivePorts');
    return Promise.resolve([]);
  }

  getCrostiniActiveNetworkInfo(): Promise<string[]> {
    this.methodCalled('getCrostiniActiveNetworkInfo');
    return Promise.resolve([]);
  }

  getCrostiniDiskInfo(vmName: string, requestFullInfo: boolean):
      Promise<CrostiniDiskInfo> {
    this.methodCalled('getCrostiniDiskInfo', vmName, requestFullInfo);
    return this.getNewPromiseFor('getCrostiniDiskInfo');
  }

  resizeCrostiniDisk(vmName: string, newSizeBytes: number): Promise<boolean> {
    this.methodCalled('resizeCrostiniDisk', vmName, newSizeBytes);
    return this.getNewPromiseFor('resizeCrostiniDisk');
  }

  checkBruschettaIsRunning(): Promise<boolean> {
    this.methodCalled('checkBruschettaIsRunning');
    return Promise.resolve(this.bruschettaIsRunning);
  }

  checkCrostiniIsRunning(): Promise<boolean> {
    this.methodCalled('checkCrostiniIsRunning');
    return Promise.resolve(this.crostiniIsRunning);
  }

  shutdownBruschetta(): void {
    this.methodCalled('shutdownBruschetta');
    this.bruschettaIsRunning = false;
  }

  shutdownCrostini(): void {
    this.methodCalled('shutdownCrostini');
    this.crostiniIsRunning = false;
  }

  setCrostiniMicSharingEnabled(enabled: boolean): void {
    this.methodCalled('setCrostiniMicSharingEnabled');
    this.crostiniMicSharingEnabled = enabled;
  }

  getCrostiniMicSharingEnabled(): Promise<boolean> {
    this.methodCalled('getCrostiniMicSharingEnabled');
    return Promise.resolve(this.crostiniMicSharingEnabled);
  }

  createContainer(
      containerId: GuestId, imageServer: string|null, imageAlias: string|null,
      containerFile: string|null): void {
    this.methodCalled(
        'createContainer', containerId, imageServer, imageAlias, containerFile);
  }

  deleteContainer(containerId: GuestId): void {
    this.methodCalled('deleteContainer', containerId);
  }

  requestContainerInfo(): void {
    this.methodCalled('requestContainerInfo');
    webUIListenerCallback('crostini-container-info', this.containerInfo);
  }

  setContainerBadgeColor(containerId: GuestId, badgeColor: SkColor): void {
    this.methodCalled('setContainerBadgeColor', [containerId, badgeColor]);
  }

  stopContainer(containerId: GuestId): void {
    this.methodCalled('stopContainer', containerId);
  }

  openContainerFileSelector(): Promise<string> {
    this.methodCalled('openContainerFileSelector');
    return Promise.resolve(this.selectedContainerFileName);
  }

  requestSharedVmDevices(): void {
    this.methodCalled('requestSharedVmDevices');
    webUIListenerCallback('crostini-shared-vmdevices', this.sharedVmDevices);
  }

  isVmDeviceShared(id: GuestId, device: string): Promise<boolean> {
    this.methodCalled('isVmDeviceShared', id, device);
    return this.getNewPromiseFor('isVmDeviceShared');
  }

  setVmDeviceShared(id: GuestId, device: string, shared: boolean):
      Promise<boolean> {
    this.methodCalled('setVmDeviceShared', id, device, shared);
    return this.getNewPromiseFor('setVmDeviceShared');
  }

  requestBruschettaInstallerView(): void {
    this.methodCalled('requestBruschettaInstallerView');
  }

  requestBruschettaUninstallerView(): void {
    this.methodCalled('requestBruschettaUninstallerView');
  }

  enableArcAdbSideload(): void {
    this.methodCalled('enableArcAdbSideload');
  }

  disableArcAdbSideload(): void {
    this.methodCalled('disableArcAdbSideload');
  }

  checkCrostiniMicSharingStatus(proposedValue: boolean): Promise<boolean> {
    this.methodCalled('checkCrostiniMicSharingStatus', proposedValue);
    return Promise.resolve(true);
  }
}
