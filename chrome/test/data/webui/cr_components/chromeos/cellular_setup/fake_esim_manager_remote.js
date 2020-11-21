// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('cellular_setup', function() {
  /** @implements {chromeos.cellularSetup.mojom.ESimProfile} */
  class FakeProfile {
    constructor(id) {
      this.properties_ = {
        activationCode: 'activation-code-' + id,
        eid: '1',
        iccid: id,
        name: 'profile' + id,
        nickname: 'profile' + id,
        serviceProvider: 'provider' + id,
        state: chromeos.cellularSetup.mojom.ProfileState.kPending,
      };
    }

    /**
     * @override
     * @return {!Promise<{properties:
     *     chromeos.cellularSetup.mojom.ESimProfileProperties},}>}
     */
    getProperties() {
      return new Promise((res) => {
        res({
          properties: this.properties_,
        });
      });
    }
  }

  /** @implements {chromeos.cellularSetup.mojom.Euicc} */
  class FakeEuicc {
    constructor(numProfiles) {
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
      return new Promise((res) => {
        res({
          result: chromeos.cellularSetup.mojom.ESimOperationResult.kSuccess
        });
      });
    }

    /**
     * @override
     * @return {!Promise<{profiles: Array<!ESimProfile>,}>}
     */
    getProfileList() {
      return new Promise((res) => {
        res({
          profiles: this.profiles_,
        });
      });
    }

    /** @private */
    addProfileForTest_() {
      const id = this.profiles_.length + 1;
      this.profiles_.push(new FakeProfile(id));
    }
  }

  /** @implements {chromeos.cellularSetup.mojom.ESimManagerInterface} */
  /* #export */ class FakeESimManagerRemote {
    constructor() {
      this.euiccs_ = [];
    }

    /**
     * @override
     * @return {!Promise<{euiccs: !Array<!Euicc>,}>}
     */
    getAvailableEuiccs() {
      return new Promise((res) => {
        res({euiccs: this.euiccs_});
      });
    }

    /**
     * @param {number} numProfiles The number of profiles the EUICC has.
     */
    addEuiccForTest(numProfiles) {
      const euicc = new FakeEuicc(numProfiles);
      this.euiccs_.push(euicc);
    }
  }

  // #cr_define_end
  return {
    FakeESimManagerRemote: FakeESimManagerRemote,
  };
});