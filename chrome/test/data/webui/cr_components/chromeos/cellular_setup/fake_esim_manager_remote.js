// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('cellular_setup', function() {
  /** @implements {chromeos.cellularSetup.mojom.ESimProfile} */
  class FakeProfile {
    constructor(id, fakeEuicc) {
      this.properties_ = {
        activationCode: 'activation-code-' + id,
        eid: '1',
        iccid: id + '',
        name: {
          data: this.stringToCharCodeArray_('profile' + id),
        },
        nickname: {
          data: this.stringToCharCodeArray_('profile' + id),
        },
        serviceProvider: {
          data: this.stringToCharCodeArray_('provider' + id),
        },
        state: chromeos.cellularSetup.mojom.ProfileState.kPending,
      };

      this.fakeEuicc_ = fakeEuicc;
    }

    /**
     * @override
     * @return {!Promise<{properties:
     *     chromeos.cellularSetup.mojom.ESimProfileProperties},}>}
     */
    getProperties() {
      return Promise.resolve({
        properties: this.properties_,
      });
    }

    /**
     * @override
     * @param {string} confirmationCode
     * @return {!Promise<{result:
     *     chromeos.cellularSetup.mojom.ProfileInstallResult},}>}
     */
    installProfile(confirmationCode) {
      this.properties_.state =
          chromeos.cellularSetup.mojom.ProfileState.kActive;
      this.fakeEuicc_.notifyProfileChangedForTest(this);
      this.fakeEuicc_.notifyProfileListChangedForTest();
      return Promise.resolve({
        result: this.profileInstallResult_ ?
            this.profileInstallResult_ :
            chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess
      });
    }

    /**
     * @param {chromeos.cellularSetup.mojom.ProfileInstallResult} result
     */
    setProfileInstallResultForTest(result) {
      this.profileInstallResult_ = result;
    }

    /**
     * @param {chromeos.cellularSetup.mojom.ESimOperationResult} result
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
      let deferred = {};
      let promise = new Promise(function(resolve, reject) {
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
     *     chromeos.cellularSetup.mojom.ESimOperationResult},}>}
     */
    setProfileNickname(nickname) {
      if (!this.esimOperationResult_ ||
          this.esimOperationResult_ ===
              chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess) {
        this.properties_.nickname = nickname;
      }

      this.deferredSetProfileNicknamePromise_ = this.deferredPromise_();
      return this.deferredSetProfileNicknamePromise_.promise;
    }

    /** @private */
    resolveSetProfileNicknamePromise_() {
      this.deferredSetProfileNicknamePromise_.resolve({
        result: this.esimOperationResult_ ?
            this.esimOperationResult_ :
            chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess
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
              chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess) {
        const removeProfileResult =
            await this.fakeEuicc_.removeProfileForTest(this.properties_.iccid);
        this.defferedUninstallProfilePromise_.resolve(removeProfileResult);
        return;
      }

      this.defferedUninstallProfilePromise_.resolve({
        result: this.esimOperationResult_ ?
            this.esimOperationResult_ :
            chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess
      });
    }
  }

  /** @implements {chromeos.cellularSetup.mojom.Euicc} */
  class FakeEuicc {
    constructor(numProfiles, fakeESimManager) {
      this.fakeESimManager_ = fakeESimManager;
      this.profiles_ = [];
      for (let i = 0; i < numProfiles; i++) {
        this.addProfileForTest_();
      }
    }

    /**
     * @override
     * @return {!Promise<{result:
     *     chromeos.cellularSetup.mojom.ESimOperationResult},}>}
     */
    requestPendingProfiles() {
      return Promise.resolve({
        result: chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess,
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
     * @param {string} activationCode
     * @param {string} confirmationCode
     * @return {!Promise<{result:
     *     chromeos.cellularSetup.mojom.ProfileInstallResult},}>}
     */
    installProfileFromActivationCode(activationCode, confirmationCode) {
      this.notifyProfileListChangedForTest();
      return Promise.resolve({
        result: this.profileInstallResult_ ?
            this.profileInstallResult_ :
            chromeos.cellularSetup.mojom.ProfileInstallResult.kSuccess,
      });
    }

    /**
     * @param {chromeos.cellularSetup.mojom.ProfileInstallResult} result
     */
    setProfileInstallResultForTest(result) {
      this.profileInstallResult_ = result;
    }

    /** @private */
    addProfileForTest_() {
      const id = this.profiles_.length + 1;
      this.profiles_.push(new FakeProfile(id, this));
    }

    /**
     * @param {string} iccid
     * @private
     */
    async removeProfileForTest(iccid) {
      const result = [];
      let profileRemoved = false;
      for (let profile of this.profiles_) {
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
        return {
          result: chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess
        };
      }
      return {
        result: chromeos.cellularSetup.mojom.ESimOperationResult.kFailure
      };
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
  }

  /** @implements {chromeos.cellularSetup.mojom.ESimManagerInterface} */
  /* #export */ class FakeESimManagerRemote {
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
     */
    addEuiccForTest(numProfiles) {
      const euicc = new FakeEuicc(numProfiles, this);
      this.euiccs_.push(euicc);
    }

    /**
     * @param {!chromeos.cellularSetup.mojom.ESimManagerObserverInterface}
     *     observer
     */
    addObserver(observer) {
      this.observers_.push(observer);
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

  // #cr_define_end
  return {
    FakeESimManagerRemote: FakeESimManagerRemote,
  };
});