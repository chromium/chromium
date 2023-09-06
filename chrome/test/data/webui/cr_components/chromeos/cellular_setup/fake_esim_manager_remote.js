// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ESimManagerInterface, ESimManagerObserverInterface, ESimOperationResult, ESimProfile, ESimProfileProperties, ESimProfileRemote, EuiccInterface, EuiccProperties, EuiccRemote, ProfileInstallMethod, ProfileInstallResult, ProfileState, QRCode} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

/** @implements {ESimProfile} */
export class FakeProfile {
  constructor(eid, iccid, fakeEuicc) {
    this.properties = {
      eid,
      iccid,
      activationCode: 'activation-code-' + iccid,
      name: {
        data: this.stringToCharCodeArray_('profile' + iccid),
      },
      nickname: {
        data: this.stringToCharCodeArray_('profile' + iccid),
      },
      serviceProvider: {
        data: this.stringToCharCodeArray_('provider' + iccid),
      },
      state: ProfileState.kPending,
    };

    this.deferGetProperties_ = false;
    this.deferredGetPropertiesPromises_ = [];
    this.fakeEuicc_ = fakeEuicc;
  }

  /**
   * @return {!Promise<{properties: ESimProfileProperties},}>}
   */
  getProperties() {
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

  /**
   * @param {boolean} defer
   */
  setDeferGetProperties(defer) {
    this.deferGetProperties_ = defer;
  }

  resolveLastGetPropertiesPromise() {
    if (!this.deferredGetPropertiesPromises_.length) {
      return;
    }
    const deferred = this.deferredGetPropertiesPromises_.pop();
    deferred.resolve({properties: this.properties});
  }

  /**
   * @param {string} confirmationCode
   * @return {!Promise<{result:
   *     ProfileInstallResult},}>}
   */
  installProfile(confirmationCode) {
    if (!this.profileInstallResult_ ||
        this.profileInstallResult_ === ProfileInstallResult.kSuccess) {
      this.properties.state = ProfileState.kActive;
    }
    this.fakeEuicc_.notifyProfileChangedForTest(this);
    this.fakeEuicc_.notifyProfileListChangedForTest();
    // Simulate a delay in response. This is neccessary because a few tests
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

  /**
   * @param {ProfileInstallResult} result
   */
  setProfileInstallResultForTest(result) {
    this.profileInstallResult_ = result;
  }

  /**
   * @param {ESimOperationResult} result
   */
  setEsimOperationResultForTest(result) {
    this.esimOperationResult_ = result;
  }

  /**
   * @param {string} string
   * @private
   */
  stringToCharCodeArray_(string) {
    const res = [];
    for (let i = 0; i < string.length; i++) {
      res.push(string.charCodeAt(i));
    }
    return res;
  }

  /**
   * @return {Object}
   * @private
   */
  deferredPromise_() {
    const deferred = {};
    const promise = new Promise(function(resolve, reject) {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });
    deferred.promise = promise;
    return deferred;
  }

  /**
   * @param {?String16} nickname
   * @return {!Promise<{result: ESimOperationResult},}>}
   */
  setProfileNickname(nickname) {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ === ESimOperationResult.kSuccess) {
      this.properties.nickname = nickname;
    }

    this.deferredSetProfileNicknamePromise_ = this.deferredPromise_();
    return this.deferredSetProfileNicknamePromise_.promise;
  }

  /** @private */
  resolveSetProfileNicknamePromise_() {
    this.deferredSetProfileNicknamePromise_.resolve({
      result: this.esimOperationResult_ ? this.esimOperationResult_ :
                                          ESimOperationResult.kSuccess,
    });
  }

  uninstallProfile() {
    this.fakeEuicc_.notifyProfileChangedForTest(this);
    this.defferedUninstallProfilePromise_ = this.deferredPromise_();
    return this.defferedUninstallProfilePromise_.promise;
  }

  /** @return {Promise<void>} */
  async resolveUninstallProfilePromise() {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ === ESimOperationResult.kSuccess) {
      const removeProfileResult =
          await this.fakeEuicc_.removeProfileForTest(this.properties.iccid);
      this.defferedUninstallProfilePromise_.resolve(removeProfileResult);
      return;
    }

    this.defferedUninstallProfilePromise_.resolve({
      result: this.esimOperationResult_ ? this.esimOperationResult_ :
                                          ESimOperationResult.kSuccess,
    });
  }
}

/** @implements {EuiccInterface} */
export class FakeEuicc {
  constructor(eid, numProfiles, fakeESimManager) {
    this.fakeESimManager_ = fakeESimManager;
    this.properties = {eid};
    this.profiles_ = [];
    for (let i = 0; i < numProfiles; i++) {
      this.addProfile();
    }
    this.requestPendingProfilesResult_ = ESimOperationResult.kSuccess;
  }

  /**
   * @return {!Promise<{properties: EuiccProperties},}>}
   */
  getProperties() {
    return Promise.resolve({properties: this.properties});
  }

  /**
   * @return {!Promise<{result:
   *     ESimOperationResult},}>}
   */
  requestPendingProfiles() {
    return Promise.resolve({
      result: this.requestPendingProfilesResult_,
    });
  }

  /**
   * @return {!Promise<{result:ESimOperationResult,
   *     profiles:Array<!ESimProfileProperties>,}}
   *
   */
  requestAvailableProfiles() {
    return Promise.resolve({
      result: this.requestPendingProfilesResult_,
      profiles: this.profiles_.map(profile => {
        return profile.properties;
      }),
    });
  }

  /**
   * @return {!Promise<{profiles: Array<!ESimProfileRemote>,}>}
   */
  getProfileList() {
    return Promise.resolve({
      profiles: this.profiles_,
    });
  }

  /**
   * @return {!Promise<{qrCode: QRCode| null}>}
   */
  getEidQRCode() {
    if (this.eidQRCode_) {
      return Promise.resolve({qrCode: this.eidQRCode_});
    } else {
      return Promise.resolve(null);
    }
  }

  /**
   * @param {string} activationCode
   * @param {string} confirmationCode
   * @param {ProfileInstallMethod} installMethod
   * @return {!Promise<{result: ProfileInstallResult, profile: ESimProfileRemote
   *     | null },}>}
   */
  installProfileFromActivationCode(
      activationCode, confirmationCode, installMethod) {
    this.notifyProfileListChangedForTest();
    return Promise.resolve({
      result: this.profileInstallResult_ ? this.profileInstallResult_ :
                                           ProfileInstallResult.kSuccess,
      profile: null,
    });
  }

  /**
   * @param {ESimOperationResult} result
   */
  setRequestPendingProfilesResult(result) {
    this.requestPendingProfilesResult_ = result;
  }

  /**
   * @param {ProfileInstallResult} result
   */
  setProfileInstallResultForTest(result) {
    this.profileInstallResult_ = result;
  }

  /**
   * @param {QRCode} qrcode
   */
  setEidQRCodeForTest(qrcode) {
    this.eidQRCode_ = qrcode;
  }

  /**
   * @param {string} iccid
   */
  async removeProfileForTest(iccid) {
    const result = [];
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

  /**
   * @param {FakeProfile} profile
   */
  notifyProfileChangedForTest(profile) {
    this.fakeESimManager_.notifyProfileChangedForTest(profile);
  }

  notifyProfileListChangedForTest() {
    this.fakeESimManager_.notifyProfileListChangedForTest(this);
  }

  /** @private */
  addProfile() {
    const iccid = this.profiles_.length + 1 + '';
    this.profiles_.push(new FakeProfile(this.properties.eid, iccid, this));
  }
}

/** @implements {ESimManagerInterface} */
export class FakeESimManagerRemote {
  constructor() {
    this.euiccs_ = [];
    this.observers_ = [];
  }

  /**
   * @return {!Promise<{euiccs: !Array<!EuiccRemote>,}>}
   */
  getAvailableEuiccs() {
    return Promise.resolve({
      euiccs: this.euiccs_,
    });
  }

  /**
   * @param {number} numProfiles The number of profiles the EUICC has.
   * @return {FakeEuicc} The euicc that was added.
   */
  addEuiccForTest(numProfiles) {
    const eid = this.euiccs_.length + 1 + '';
    const euicc = new FakeEuicc(eid, numProfiles, this);
    this.euiccs_.push(euicc);
    this.notifyAvailableEuiccListChanged();
    return euicc;
  }

  /**
   * @param {!ESimManagerObserverInterface} observer
   */
  addObserver(observer) {
    this.observers_.push(observer);
  }

  notifyAvailableEuiccListChanged() {
    for (const observer of this.observers_) {
      observer.onAvailableEuiccListChanged();
    }
  }

  /**
   * @param {FakeEuicc} euicc
   */
  notifyProfileListChangedForTest(euicc) {
    for (const observer of this.observers_) {
      observer.onProfileListChanged(euicc);
    }
  }

  /**
   * @param {FakeProfile|null} profile
   */
  notifyProfileChangedForTest(profile) {
    for (const observer of this.observers_) {
      observer.onProfileChanged(profile);
    }
  }
}
