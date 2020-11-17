// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('cellular_setup', function() {
  /** @implements {chromeos.cellularSetup.mojom.ESimManagerInterface} */
  /* #export */ class FakeESimManagerRemote {
    /**
     * @override
     * @return {!Promise<{euiccs: !Array<!Euicc>,}>}
     */
    getAvailableEuiccs() {
      return new Promise((res) => {
        res({
          euiccs: [{
            eid: '1',
            isActive: true,
          }]
        });
      });
    }

    /**
     * @override
     * @param { !string } eid
     * @return {!Promise<{profiles: Array<!ESimProfile>,}>}
     */
    getProfiles(eid) {
      return new Promise((res) => {
        res({
          profiles: [{
            activationCode: 'activation-code-1',
            eid: '1',
            iccid: '1',
            name: 'profile1',
            nickname: 'profile1',
            serviceProvider: 'provider1',
            state: chromeos.cellularSetup.mojom.ProfileState.kPending,
          }]
        });
      });
    }
  }

  // #cr_define_end
  return {
    FakeESimManagerRemote: FakeESimManagerRemote,
  };
});