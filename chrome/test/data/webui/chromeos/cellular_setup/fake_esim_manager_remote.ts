// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import type {ESimManagerInterface, ESimManagerObserverInterface, ESimProfileInterface, ESimProfileProperties, ESimProfileRemote, EuiccInterface, EuiccProperties, EuiccRemote, ProfileInstallMethod, QRCode} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {ESimOperationResult, ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

interface DeferredPromiseObject {
  promise: Promise<any>;
  resolve: Function;
  reject: Function;
}

export class FakeProfile implements ESimProfileInterface {
  properties: ESimProfileProperties;
  private deferGetProperties_: boolean;
  private deferredGetPropertiesPromises_: DeferredPromiseObject[];
  private fakeEuicc_: FakeEuicc;
  private profileInstallResult_: ProfileInstallResult|null = null;
  private esimOperationResult_: ESimOperationResult|null = null;
  private deferredSetProfileNicknamePromise_: DeferredPromiseObject|null = null;
  private deferedUninstallProfilePromise_: DeferredPromiseObject|null = null;

  constructor(eid: string, iccid: string, fakeEuicc: FakeEuicc) {
    this.properties = {
      eid: eid,
      iccid: iccid,
      activationCode: 'activation-code-' + iccid,
      name: stringToMojoString16('profile' + iccid),
      nickname: stringToMojoString16('profile' + iccid),
      serviceProvider: stringToMojoString16('provider' + iccid),
      state: ProfileState.kPending,
    };

    this.deferGetProperties_ = false;
    this.deferredGetPropertiesPromises_ = [];
    this.fakeEuicc_ = fakeEuicc;
  }

  getProperties(): Promise<{properties: ESimProfileProperties}> {
    if (this.deferGetProperties_) {
      const deferred = this.deferredPromise_();
      this.deferredGetPropertiesPromises_.push(deferred);
      return deferred.promise;
    } else {
      return Promise.resolve({
        properties: this.properties,
      });
    }
  }

  setDeferGetProperties(defer: boolean): void {
    this.deferGetProperties_ = defer;
  }

  resolveLastGetPropertiesPromise(): void {
    if (!this.deferredGetPropertiesPromises_.length) {
      return;
    }
    const deferred = this.deferredGetPropertiesPromises_.pop();
    deferred!.resolve({properties: this.properties});
  }

  installProfile(_confirmationCode: string):
      Promise<{result: ProfileInstallResult}> {
    if (!this.profileInstallResult_ ||
        this.profileInstallResult_ === ProfileInstallResult.kSuccess) {
      this.properties.state = ProfileState.kActive;
    }
    this.fakeEuicc_.notifyProfileChangedForTest(this);
    this.fakeEuicc_.notifyProfileListChangedForTest();
    // Simulate a delay in response. This is necessary because a few tests
    // require UI to be in installing state.
    return new Promise(
        resolve => setTimeout(
            () => resolve({
              result: this.profileInstallResult_ ?
                  this.profileInstallResult_ :
                  ProfileInstallResult.kSuccess,
            }),
            0));
  }

  setProfileInstallResultForTest(result: ProfileInstallResult) {
    this.profileInstallResult_ = result;
  }

  setEsimOperationResultForTest(result: ESimOperationResult) {
    this.esimOperationResult_ = result;
  }

  private deferredPromise_(): DeferredPromiseObject {
    let resolve_out!: Function;
    let reject_out!: Function;
    const promise = new Promise((resolve, reject) => {
      resolve_out = resolve;
      reject_out = reject;
    });
    return {
      promise: promise,
      resolve: resolve_out,
      reject: reject_out,
    };
  }

  setProfileNickname(nickname: String16):
      Promise<{result: ESimOperationResult}> {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ === ESimOperationResult.kSuccess) {
      this.properties.nickname = nickname;
    }

    this.deferredSetProfileNicknamePromise_ = this.deferredPromise_();
    return this.deferredSetProfileNicknamePromise_.promise;
  }

  resolveSetProfileNicknamePromise(): void {
    this.deferredSetProfileNicknamePromise_!.resolve({
      result: this.esimOperationResult_ ? this.esimOperationResult_ :
                                          ESimOperationResult.kSuccess,
    });
  }

  uninstallProfile(): Promise<{result: number}> {
    this.fakeEuicc_.notifyProfileChangedForTest(this);
    this.deferedUninstallProfilePromise_ = this.deferredPromise_();
    return this.deferedUninstallProfilePromise_.promise;
  }

  async resolveUninstallProfilePromise(): Promise<void> {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ === ESimOperationResult.kSuccess) {
      const removeProfileResult =
          await this.fakeEuicc_.removeProfileForTest(this.properties.iccid);
      this.deferedUninstallProfilePromise_!.resolve(removeProfileResult);
      return;
    }

    this.deferedUninstallProfilePromise_!.resolve({
      result: this.esimOperationResult_ ? this.esimOperationResult_ :
                                          ESimOperationResult.kSuccess,
    });
  }
}

export class FakeEuicc implements EuiccInterface {
  properties: EuiccProperties;
  private fakeESimManager_: FakeESimManagerRemote;
  private profiles_: FakeProfile[];
  private refreshInstalledProfilesResult_: ESimOperationResult;
  private refreshInstalledProfilesCount_: number = 0;
  private requestPendingProfilesResult_: ESimOperationResult;
  private eidQRCode_: QRCode|null = null;
  private profileInstallResult_: ProfileInstallResult|null = null;

  constructor(
      eid: string, numProfiles: number,
      fakeESimManager: FakeESimManagerRemote) {
    this.fakeESimManager_ = fakeESimManager;
    this.properties = {eid: eid, isActive: false};
    this.profiles_ = [];
    for (let i = 0; i < numProfiles; i++) {
      this.addProfile();
    }
    this.refreshInstalledProfilesResult_ = ESimOperationResult.kSuccess;
    this.requestPendingProfilesResult_ = ESimOperationResult.kSuccess;
  }

  getProperties(): Promise<{properties: EuiccProperties}> {
    return Promise.resolve({properties: this.properties});
  }

  requestPendingProfiles(): Promise<{result: ESimOperationResult}> {
    // Requesting pending profiles refreshes the installed profile list.
    this.refreshInstalledProfilesCount_++;
    return Promise.resolve({
      result: this.requestPendingProfilesResult_,
    });
  }

  requestAvailableProfiles(): Promise<{
    result: ESimOperationResult,
    profiles: ESimProfileProperties[],
  }> {
    return Promise.resolve({
      result: this.requestPendingProfilesResult_,
      profiles: this.profiles_.map(profile => {
        return profile.properties;
      }),
    });
  }

  refreshInstalledProfiles(): Promise<{result: ESimOperationResult}> {
    this.refreshInstalledProfilesCount_++;
    return Promise.resolve({
      result: this.refreshInstalledProfilesResult_,
    });
  }

  getProfileList(): Promise<{profiles: ESimProfileRemote[]}> {
    return Promise.resolve({
      profiles: this.profiles_ as unknown as ESimProfileRemote[],
    });
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  getEidQRCode(): Promise<{qrCode: QRCode | null}> {
    if (this.eidQRCode_) {
      return Promise.resolve({qrCode: this.eidQRCode_});
    } else {
      return Promise.resolve({qrCode: null});
    }
  }

  getRefreshInstalledProfilesCount(): number {
    return this.refreshInstalledProfilesCount_;
  }

  installProfileFromActivationCode(
      _activationCode: string, _confirmationCode: string,
      _installMethod: ProfileInstallMethod):
      Promise<{result: ProfileInstallResult, profile: ESimProfileRemote|null}> {
    this.notifyProfileListChangedForTest();
    return Promise.resolve({
      result: this.profileInstallResult_ ? this.profileInstallResult_ :
                                           ProfileInstallResult.kSuccess,
      profile: null,
    });
  }

  setRequestPendingProfilesResult(result: ESimOperationResult): void {
    this.requestPendingProfilesResult_ = result;
  }

  setProfileInstallResultForTest(result: ProfileInstallResult): void {
    this.profileInstallResult_ = result;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  setEidQRCodeForTest(qrcode: QRCode): void {
    this.eidQRCode_ = qrcode;
  }

  async removeProfileForTest(iccid: string) {
    const result: FakeProfile[] = [];
    let profileRemoved = false;
    for (const profile of this.profiles_) {
      const property = await profile.getProperties();
      if (property.properties.iccid === iccid) {
        profileRemoved = true;
        continue;
      }
      result.push(profile);
    }
    this.profiles_ = result;

    if (profileRemoved) {
      this.notifyProfileListChangedForTest();
      return {result: ESimOperationResult.kSuccess};
    }
    return {result: ESimOperationResult.kFailure};
  }

  notifyProfileChangedForTest(profile: FakeProfile): void {
    this.fakeESimManager_.notifyProfileChangedForTest(profile);
  }

  notifyProfileListChangedForTest(): void {
    this.fakeESimManager_.notifyProfileListChangedForTest(this);
  }

  addProfile(): void {
    const iccid = this.profiles_.length + 1 + '';
    this.profiles_.push(new FakeProfile(this.properties.eid, iccid, this));
  }
}

// eslint-disable-next-line @typescript-eslint/naming-convention
export class FakeESimManagerRemote implements ESimManagerInterface {
  private euiccs_: FakeEuicc[];
  private observers_: ESimManagerObserverInterface[];

  constructor() {
    this.euiccs_ = [];
    this.observers_ = [];
  }

  getAvailableEuiccs(): Promise<{euiccs: EuiccRemote[]}> {
    return Promise.resolve({
      euiccs: this.euiccs_ as unknown as EuiccRemote[],
    });
  }

  addEuiccForTest(numProfiles: number): FakeEuicc {
    const eid = this.euiccs_.length + 1 + '';
    const euicc = new FakeEuicc(eid, numProfiles, this);
    this.euiccs_.push(euicc);
    this.notifyAvailableEuiccListChanged();
    return euicc;
  }

  addObserver(observer: ESimManagerObserverInterface): void {
    this.observers_.push(observer);
  }

  notifyAvailableEuiccListChanged(): void {
    for (const observer of this.observers_) {
      observer.onAvailableEuiccListChanged();
    }
  }

  notifyProfileListChangedForTest(euicc: FakeEuicc): void {
    for (const observer of this.observers_) {
      observer.onProfileListChanged(euicc as unknown as EuiccRemote);
    }
  }

  notifyProfileChangedForTest(profile: FakeProfile|null): void {
    for (const observer of this.observers_) {
      observer.onProfileChanged(profile as unknown as ESimProfileRemote);
    }
  }
}
