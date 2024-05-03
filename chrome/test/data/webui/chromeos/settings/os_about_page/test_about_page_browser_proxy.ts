// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AboutPageBrowserProxy, BrowserChannel, ChannelInfo, EndOfLifeInfo, RegulatoryInfo, TpmFirmwareUpdateStatusChangedEvent, UpdateStatus, VersionInfo} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAboutPageBrowserProxy extends TestBrowserProxy implements
    AboutPageBrowserProxy {
  /* Used from tests to delay the resolving of the getChannelInfo() method. */
  fakeChannelInfoDelay: PromiseResolver<unknown>|null = null;

  private updateStatus_: UpdateStatus = UpdateStatus.UPDATED;
  private sendUpdateStatus_ = true;
  private versionInfo_: VersionInfo = {
    arcVersion: '',
    osFirmware: '',
    osVersion: '',
  };
  private channelInfo_: ChannelInfo = {
    currentChannel: BrowserChannel.BETA,
    targetChannel: BrowserChannel.BETA,
    isLts: false,
  };
  private canChangeChannel_ = true;
  private regulatoryInfo_: RegulatoryInfo|null = null;
  private tpmFirmwareUpdateStatus_: TpmFirmwareUpdateStatusChangedEvent = {
    updateAvailable: false,
  };
  private endOfLifeInfo_: EndOfLifeInfo = {
    hasEndOfLife: false,
    aboutPageEndOfLifeMessage: '',
    shouldShowEndOfLifeIncentive: false,
    shouldShowOfferText: false,
    isExtendedUpdatesDatePassed: false,
    isExtendedUpdatesOptInRequired: false,
  };
  private hasInternetConnection_ = true;
  private managedAutoUpdateEnabled_ = true;
  private consumerAutoUpdateEnabled_ = true;
  private firmwareUpdateCount_ = 0;
  private extendedUpdatesOptInEligible_ = false;

  constructor() {
    super([
      'applyDeferredUpdate',
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
      'endOfLifeIncentiveButtonClicked',
      'launchReleaseNotes',
      'openDiagnostics',
      'openOsHelpPage',
      'openProductLicenseOther',
      'refreshTpmFirmwareUpdateStatus',
      'requestUpdate',
      'requestUpdateOverCellular',
      'setChannel',
      'getFirmwareUpdateCount',
      'openFirmwareUpdatesPage',
      'isManagedAutoUpdateEnabled',
      'isConsumerAutoUpdateEnabled',
      'setConsumerAutoUpdate',
      'isExtendedUpdatesOptInEligible',
      'openExtendedUpdatesDialog',
      'recordExtendedUpdatesShown',
    ]);
  }

  setUpdateStatus(updateStatus: UpdateStatus): void {
    this.updateStatus_ = updateStatus;
  }

  blockRefreshUpdateStatus(): void {
    this.sendUpdateStatus_ = false;
  }

  sendStatusNoInternet(): void {
    webUIListenerCallback('update-status-changed', {
      progress: 0,
      status: UpdateStatus.FAILED,
      message: 'offline',
      connectionTypes: 'no internet',
    });
  }

  setManagedAutoUpdate(enabled: boolean): void {
    this.managedAutoUpdateEnabled_ = enabled;
  }

  resetConsumerAutoUpdate(enabled: boolean): void {
    this.consumerAutoUpdateEnabled_ = enabled;
  }

  pageReady(): void {
    this.methodCalled('pageReady');
  }

  refreshUpdateStatus(): void {
    if (this.sendUpdateStatus_) {
      webUIListenerCallback('update-status-changed', {
        progress: 1,
        status: this.updateStatus_,
      });
    }
    this.methodCalled('refreshUpdateStatus');
  }

  openFeedbackDialog(): void {
    this.methodCalled('openFeedbackDialog');
  }

  openHelpPage(): void {
    this.methodCalled('openHelpPage');
  }

  setVersionInfo(versionInfo: VersionInfo): void {
    this.versionInfo_ = versionInfo;
  }

  setCanChangeChannel(canChangeChannel: boolean): void {
    this.canChangeChannel_ = canChangeChannel;
  }

  setChannels(current: BrowserChannel, target: BrowserChannel): void {
    this.channelInfo_.currentChannel = current;
    this.channelInfo_.targetChannel = target;
  }

  setRegulatoryInfo(regulatoryInfo: RegulatoryInfo|null): void {
    this.regulatoryInfo_ = regulatoryInfo;
  }

  setEndOfLifeInfo(endOfLifeInfo: EndOfLifeInfo): void {
    this.endOfLifeInfo_ = endOfLifeInfo;
  }

  setInternetConnection(hasInternetConnection: boolean): void {
    this.hasInternetConnection_ = hasInternetConnection;
  }

  getVersionInfo(): Promise<VersionInfo> {
    this.methodCalled('getVersionInfo');
    return Promise.resolve(this.versionInfo_);
  }

  getVersionInfoForTesting(): VersionInfo {
    return this.versionInfo_;
  }

  async getChannelInfo(): Promise<ChannelInfo> {
    if (this.fakeChannelInfoDelay) {
      await this.fakeChannelInfoDelay;
    }
    this.methodCalled('getChannelInfo');
    return this.channelInfo_;
  }

  getChannelInfoForTesting(): ChannelInfo {
    return this.channelInfo_;
  }

  canChangeChannel(): Promise<boolean> {
    this.methodCalled('canChangeChannel');
    return Promise.resolve(this.canChangeChannel_);
  }

  checkInternetConnection(): Promise<boolean> {
    this.methodCalled('checkInternetConnection');
    return Promise.resolve(this.hasInternetConnection_);
  }

  getRegulatoryInfo(): Promise<RegulatoryInfo|null> {
    this.methodCalled('getRegulatoryInfo');
    return Promise.resolve(this.regulatoryInfo_);
  }

  getEndOfLifeInfo(): Promise<EndOfLifeInfo> {
    this.methodCalled('getEndOfLifeInfo');
    return Promise.resolve(this.endOfLifeInfo_);
  }

  endOfLifeIncentiveButtonClicked(): void {
    this.methodCalled('endOfLifeIncentiveButtonClicked');
  }

  setChannel(channel: BrowserChannel, isPowerwashAllowed: boolean): void {
    this.methodCalled('setChannel', [channel, isPowerwashAllowed]);
  }

  setTpmFirmwareUpdateStatus(status: TpmFirmwareUpdateStatusChangedEvent):
      void {
    this.tpmFirmwareUpdateStatus_ = status;
  }

  refreshTpmFirmwareUpdateStatus(): void {
    this.methodCalled('refreshTpmFirmwareUpdateStatus');
    webUIListenerCallback(
        'tpm-firmware-update-status-changed', this.tpmFirmwareUpdateStatus_);
  }

  requestUpdate(): void {
    this.setUpdateStatus(UpdateStatus.UPDATING);
    this.refreshUpdateStatus();
    this.methodCalled('requestUpdate');
  }

  openOsHelpPage(): void {
    this.methodCalled('openOsHelpPage');
  }

  openDiagnostics(): void {
    this.methodCalled('openDiagnostics');
  }

  launchReleaseNotes(): void {
    this.methodCalled('launchReleaseNotes');
  }

  openFirmwareUpdatesPage(): void {
    this.methodCalled('openFirmwareUpdatesPage');
  }

  getFirmwareUpdateCount(): Promise<number> {
    this.methodCalled('getFirmwareUpdateCount');
    return Promise.resolve(this.firmwareUpdateCount_);
  }

  setFirmwareUpdatesCount(firmwareUpdatesCount: number): void {
    this.firmwareUpdateCount_ = firmwareUpdatesCount;
  }

  isManagedAutoUpdateEnabled(): Promise<boolean> {
    this.methodCalled('isManagedAutoUpdateEnabled');
    return Promise.resolve(this.managedAutoUpdateEnabled_);
  }

  isConsumerAutoUpdateEnabled(): Promise<boolean> {
    this.methodCalled('isConsumerAutoUpdateEnabled');
    return Promise.resolve(this.consumerAutoUpdateEnabled_);
  }

  setConsumerAutoUpdate(enable: boolean): void {
    this.consumerAutoUpdateEnabled_ = enable;
    this.methodCalled('setConsumerAutoUpdate');
  }

  setExtendedUpdatesOptInEligible(eligible: boolean): void {
    this.extendedUpdatesOptInEligible_ = eligible;
  }

  isExtendedUpdatesOptInEligible(
      eolPassed: boolean, extendedDatePassed: boolean,
      extendedOptInRequired: boolean): Promise<boolean> {
    this.methodCalled(
        'isExtendedUpdatesOptInEligible', eolPassed, extendedDatePassed,
        extendedOptInRequired);
    return Promise.resolve(this.extendedUpdatesOptInEligible_);
  }

  openExtendedUpdatesDialog(): void {
    this.methodCalled('openExtendedUpdatesDialog');
  }

  recordExtendedUpdatesShown(): void {
    this.methodCalled('recordExtendedUpdatesShown');
  }

  applyDeferredUpdate(): void {
    this.methodCalled('applyDeferredUpdate');
  }

  openProductLicenseOther(): void {
    this.methodCalled('openProductLicenseOther');
  }

  requestUpdateOverCellular(targetVersion: string, targetSize: string): void {
    this.methodCalled('requestUpdateOverCellular', [targetVersion, targetSize]);
  }
}
