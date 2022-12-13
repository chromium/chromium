// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserChannel, UpdateStatus} from 'chrome://os-settings/chromeos/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/** @implements {AboutPageBrowserProxy} */
export class TestAboutPageBrowserProxyChromeOS extends TestBrowserProxy {
  constructor() {
    super([
      'pageReady',
      'refreshUpdateStatus',
      'openHelpPage',
      'openFeedbackDialog',
      'canChangeChannel',
      'getChannelInfo',
      'getVersionInfo',
      'getRegulatoryInfo',
      'checkInternetConnection',
      'getEndOfLifeInfo',
      'launchReleaseNotes',
      'openOsHelpPage',
      'openDiagnostics',
      'refreshTpmFirmwareUpdateStatus',
      'requestUpdate',
      'setChannel',
      'getFirmwareUpdateCount',
      'openFirmwareUpdatesPage',
      'isManagedAutoUpdateEnabled',
      'isConsumerAutoUpdateEnabled',
      'setConsumerAutoUpdate',
    ]);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;

    /** @private {!boolean} */
    this.sendUpdateStatus_ = true;

    /** @private {!VersionInfo} */
    this.versionInfo_ = {
      arcVersion: '',
      osFirmware: '',
      osVersion: '',
    };

    /** @private {!ChannelInfo} */
    this.channelInfo_ = {
      currentChannel: BrowserChannel.BETA,
      targetChannel: BrowserChannel.BETA,
    };

    /** @private {!boolean} */
    this.canChangeChannel_ = true;

    /** @private {?RegulatoryInfo} */
    this.regulatoryInfo_ = null;

    /** @private {!TPMFirmwareUpdateStatus} */
    this.tpmFirmwareUpdateStatus_ = {
      updateAvailable: false,
    };

    this.endOfLifeInfo_ = {
      hasEndOfLife: false,
      aboutPageEndOfLifeMessage: '',
    };

    /** @private {!boolean} */
    this.managedAutoUpdateEnabled_ = true;

    /** @private {!boolean} */
    this.consumerAutoUpdateEnabled_ = true;

    /** @private {number} */
    this.firmwareUpdateCount_ = 0;
  }

  /** @param {!UpdateStatus} updateStatus */
  setUpdateStatus(updateStatus) {
    this.updateStatus_ = updateStatus;
  }

  blockRefreshUpdateStatus() {
    this.sendUpdateStatus_ = false;
  }

  sendStatusNoInternet() {
    webUIListenerCallback('update-status-changed', {
      progress: 0,
      status: UpdateStatus.FAILED,
      message: 'offline',
      connectionTypes: 'no internet',
    });
  }

  /** @param {boolean} enabled */
  setManagedAutoUpdate(enabled) {
    this.managedAutoUpdateEnabled_ = enabled;
  }

  /** @param {boolean} enabled */
  resetConsumerAutoUpdate(enabled) {
    this.consumerAutoUpdateEnabled_ = enabled;
  }

  /** @override */
  pageReady() {
    this.methodCalled('pageReady');
  }

  /** @override */
  refreshUpdateStatus() {
    if (this.sendUpdateStatus_) {
      webUIListenerCallback('update-status-changed', {
        progress: 1,
        status: this.updateStatus_,
      });
    }
    this.methodCalled('refreshUpdateStatus');
  }

  /** @override */
  openFeedbackDialog() {
    this.methodCalled('openFeedbackDialog');
  }

  /** @override */
  openHelpPage() {
    this.methodCalled('openHelpPage');
  }

  /** @param {!VersionInfo} */
  setVersionInfo(versionInfo) {
    this.versionInfo_ = versionInfo;
  }

  /** @param {boolean} canChangeChannel */
  setCanChangeChannel(canChangeChannel) {
    this.canChangeChannel_ = canChangeChannel;
  }

  /**
   * @param {!BrowserChannel} current
   * @param {!BrowserChannel} target
   */
  setChannels(current, target) {
    this.channelInfo_.currentChannel = current;
    this.channelInfo_.targetChannel = target;
  }

  /** @param {?RegulatoryInfo} regulatoryInfo */
  setRegulatoryInfo(regulatoryInfo) {
    this.regulatoryInfo_ = regulatoryInfo;
  }

  /** @param {!EndOfLifeInfo} endOfLifeInfo */
  setEndOfLifeInfo(endOfLifeInfo) {
    this.endOfLifeInfo_ = endOfLifeInfo;
  }

  /** @param {boolean|Promise} hasInternetConnection */
  setInternetConnection(hasInternetConnection) {
    this.hasInternetConnection_ = hasInternetConnection;
  }

  /** @override */
  getVersionInfo() {
    this.methodCalled('getVersionInfo');
    return Promise.resolve(this.versionInfo_);
  }

  /** @override */
  getChannelInfo() {
    this.methodCalled('getChannelInfo');
    return Promise.resolve(this.channelInfo_);
  }

  /** @override */
  canChangeChannel() {
    this.methodCalled('canChangeChannel');
    return Promise.resolve(this.canChangeChannel_);
  }

  /** @override */
  checkInternetConnection() {
    this.methodCalled('checkInternetConnection');
    return Promise.resolve(this.hasInternetConnection_);
  }

  /** @override */
  getRegulatoryInfo() {
    this.methodCalled('getRegulatoryInfo');
    return Promise.resolve(this.regulatoryInfo_);
  }

  /** @override */
  getEndOfLifeInfo() {
    this.methodCalled('getEndOfLifeInfo');
    return Promise.resolve(this.endOfLifeInfo_);
  }

  /** @override */
  setChannel(channel, isPowerwashAllowed) {
    this.methodCalled('setChannel', [channel, isPowerwashAllowed]);
  }

  /** @param {!TPMFirmwareUpdateStatus} status */
  setTPMFirmwareUpdateStatus(status) {
    this.tpmFirmwareUpdateStatus_ = status;
  }

  /** @override */
  refreshTpmFirmwareUpdateStatus() {
    this.methodCalled('refreshTpmFirmwareUpdateStatus');
    webUIListenerCallback(
        'tpm-firmware-update-status-changed', this.tpmFirmwareUpdateStatus_);
  }

  /** @override */
  requestUpdate() {
    this.setUpdateStatus(UpdateStatus.UPDATING);
    this.refreshUpdateStatus();
    this.methodCalled('requestUpdate');
  }

  /** @override */
  openOsHelpPage() {
    this.methodCalled('openOsHelpPage');
  }

  /** @override */
  openDiagnostics() {
    this.methodCalled('openDiagnostics');
  }

  /** @override */
  launchReleaseNotes() {
    this.methodCalled('launchReleaseNotes');
  }

  /** @override */
  openFirmwareUpdatesPage() {
    this.methodCalled('openFirmwareUpdatesPage');
  }

  /** @override */
  getFirmwareUpdateCount() {
    this.methodCalled('getFirmwareUpdateCount');
    return Promise.resolve(this.firmwareUpdateCount_);
  }

  /** @param {number} firmwareUpdatesCount */
  setFirmwareUpdatesCount(firmwareUpdatesCount) {
    this.firmwareUpdateCount_ = firmwareUpdatesCount;
  }

  /** @override */
  isManagedAutoUpdateEnabled() {
    this.methodCalled('isManagedAutoUpdateEnabled');
    return Promise.resolve(this.managedAutoUpdateEnabled_);
  }

  /** @override */
  isConsumerAutoUpdateEnabled() {
    this.methodCalled('isConsumerAutoUpdateEnabled');
    return Promise.resolve(this.consumerAutoUpdateEnabled_);
  }

  /** @override */
  setConsumerAutoUpdate(enable) {
    this.consumerAutoUpdateEnabled_ = enable;
    this.methodCalled('setConsumerAutoUpdate');
  }
}
