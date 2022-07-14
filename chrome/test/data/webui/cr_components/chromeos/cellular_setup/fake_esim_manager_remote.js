// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {ash.cellularSetup.mojom.ESimProfile} */
class FakeProfile {
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
      state: ash.cellularSetup.mojom.ProfileState.kPending,
    };

    this.deferGetProperties_ = false;
    this.deferredGetPropertiesPromises_ = [];
    this.fakeEuicc_ = fakeEuicc;
  }

  /**
   * @override
   * @return {!Promise<{properties:
   *     ash.cellularSetup.mojom.ESimProfileProperties},}>}
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
   * @override
   * @param {string} confirmationCode
   * @return {!Promise<{result:
   *     ash.cellularSetup.mojom.ProfileInstallResult},}>}
   */
  installProfile(confirmationCode) {
    if (!this.profileInstallResult_ ||
        this.profileInstallResult_ ===
            ash.cellularSetup.mojom.ProfileInstallResult.kSuccess) {
      this.properties.state = ash.cellularSetup.mojom.ProfileState.kActive;
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
                  ash.cellularSetup.mojom.ProfileInstallResult.kSuccess,
            }),
            0));
  }

  /**
   * @param {ash.cellularSetup.mojom.ProfileInstallResult} result
   */
  setProfileInstallResultForTest(result) {
    this.profileInstallResult_ = result;
  }

  /**
   * @param {ash.cellularSetup.mojom.ESimOperationResult} result
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
   * @override
   * @param {?mojoBase.mojom.String16} nickname
   * @return {!Promise<{result:
   *     ash.cellularSetup.mojom.ESimOperationResult},}>}
   */
  setProfileNickname(nickname) {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ ===
            ash.cellularSetup.mojom.ESimOperationResult.kSuccess) {
      this.properties.nickname = nickname;
    }

    this.deferredSetProfileNicknamePromise_ = this.deferredPromise_();
    return this.deferredSetProfileNicknamePromise_.promise;
  }

  /** @private */
  resolveSetProfileNicknamePromise_() {
    this.deferredSetProfileNicknamePromise_.resolve({
      result: this.esimOperationResult_ ?
          this.esimOperationResult_ :
          ash.cellularSetup.mojom.ESimOperationResult.kSuccess,
    });
  }

  /** @override */
  uninstallProfile() {
    this.fakeEuicc_.notifyProfileChangedForTest(this);
    this.defferedUninstallProfilePromise_ = this.deferredPromise_();
    return this.defferedUninstallProfilePromise_.promise;
  }

  /** @return {Promise<void>} */
  async resolveUninstallProfilePromise() {
    if (!this.esimOperationResult_ ||
        this.esimOperationResult_ ===
            ash.cellularSetup.mojom.ESimOperationResult.kSuccess) {
      const removeProfileResult =
          await this.fakeEuicc_.removeProfileForTest(this.properties.iccid);
      this.defferedUninstallProfilePromise_.resolve(removeProfileResult);
      return;
    }

    this.defferedUninstallProfilePromise_.resolve({
      result: this.esimOperationResult_ ?
          this.esimOperationResult_ :
          ash.cellularSetup.mojom.ESimOperationResult.kSuccess,
    });
  }
}

/** @implements {ash.cellularSetup.mojom.Euicc} */
class FakeEuicc {
  constructor(eid, numProfiles, fakeESimManager) {
    this.fakeESimManager_ = fakeESimManager;
    this.properties = {eid};
    this.profiles_ = [];
    for (let i = 0; i < numProfiles; i++) {
      this.addProfile();
    }
    this.requestPendingProfilesResult_ =
        ash.cellularSetup.mojom.ESimOperationResult.kSuccess;
  }

  /**
   * @override
   * @return {!Promise<{properties:
   *     ash.cellularSetup.mojom.EuiccProperties},}>}
   */
  getProperties() {
    return Promise.resolve({properties: this.properties});
  }

  /**
   * @override
   * @return {!Promise<{result:
   *     ash.cellularSetup.mojom.ESimOperationResult},}>}
   */
  requestPendingProfiles() {
    return Promise.resolve({
      result: this.requestPendingProfilesResult_,
    });
  }

  /**
   * @override
   * @return {!Promise<{profiles: Array<!ESimProfile>,}>}
   */
  getProfileList() {
    return Promise.resolve({
      profiles: this.profiles_,
    });
  }

  /**
   * @override
   * @return {!Promise<{qrCode: ash.cellularSetup.mojom.QRCode} | null>}
   */
  getEidQRCode() {
    if (this.eidQRCode_) {
      return Promise.resolve({qrCode: this.eidQRCode_});
    } else {
      return Promise.resolve(null);
    }
  }

  /**
   * @override
   * @param {string} activationCode
   * @param {string} confirmationCode
   * @param {boolean} isInstallViaQrCode
   * @return {!Promise<{result:
   *     ash.cellularSetup.mojom.ProfileInstallResult},}>}
   */
  installProfileFromActivationCode(
      activationCode, confirmationCode, isInstallViaQrCode) {
    this.notifyProfileListChangedForTest();
    return Promise.resolve({
      result: this.profileInstallResult_ ?
          this.profileInstallResult_ :
          ash.cellularSetup.mojom.ProfileInstallResult.kSuccess,
    });
  }

  /**
   * @param {ash.cellularSetup.mojom.ESimOperationResult} result
   */
  setRequestPendingProfilesResult(result) {
    this.requestPendingProfilesResult_ = result;
  }

  /**
   * @param {ash.cellularSetup.mojom.ProfileInstallResult} result
   */
  setProfileInstallResultForTest(result) {
    this.profileInstallResult_ = result;
  }

  /**
   * @param {ash.cellularSetup.mojom.QRCode} qrcode
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
      return {result: ash.cellularSetup.mojom.ESimOperationResult.kSuccess};
    }
    return {result: ash.cellularSetup.mojom.ESimOperationResult.kFailure};
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

/** @implements {ash.cellularSetup.mojom.ESimManagerInterface} */
export class FakeESimManagerRemote {
  constructor() {
    this.euiccs_ = [];
    this.observers_ = [];
  }

  /**
   * @override
   * @return {!Promise<{euiccs: !Array<!Euicc>,}>}
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
   * @param {!ash.cellularSetup.mojom.ESimManagerObserverInterface} observer
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
   * @param {FakeProfile} profile
   */
  notifyProfileChangedForTest(profile) {
    for (const observer of this.observers_) {
      observer.onProfileChanged(profile);
    }
  }
}
