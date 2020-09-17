// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {BrowserChannel,UpdateStatus} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

/** @implements {settings.AboutPageBrowserProxy} */
/* #export */ class TestAboutPageBrowserProxyChromeOS extends TestBrowserProxy {
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
      'refreshTPMFirmwareUpdateStatus',
      'setChannel',
    ]);

    /** @private {!UpdateStatus} */
    this.updateStatus_ = UpdateStatus.UPDATED;

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
  }

  /** @param {!UpdateStatus} updateStatus */
  setUpdateStatus(updateStatus) {
    this.updateStatus_ = updateStatus;
  }

  sendStatusNoInternet() {
    cr.webUIListenerCallback('update-status-changed', {
      progress: 0,
      status: UpdateStatus.FAILED,
      message: 'offline',
      connectionTypes: 'no internet',
    });
  }

  /** @override */
  pageReady() {
    this.methodCalled('pageReady');
  }

  /** @override */
  refreshUpdateStatus() {
    cr.webUIListenerCallback('update-status-changed', {
      progress: 1,
      status: this.updateStatus_,
    });
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
  refreshTPMFirmwareUpdateStatus() {
    this.methodCalled('refreshTPMFirmwareUpdateStatus');
    cr.webUIListenerCallback(
        'tpm-firmware-update-status-changed', this.tpmFirmwareUpdateStatus_);
  }

  /** @override */
  openOsHelpPage() {
    this.methodCalled('openOsHelpPage');
  }

  /** @override */
  launchReleaseNotes() {
    this.methodCalled('launchReleaseNotes');
  }
}
