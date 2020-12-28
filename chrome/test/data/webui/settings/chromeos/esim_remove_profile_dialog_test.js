// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// clang-format on

suite('EsimRemoveProfileDialog', function() {
  let esimRemoveProfileDialog;
  let eSimManagerRemote;
  let mojoApi_;


  setup(function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();
  });

  async function init(iccid) {
    esimRemoveProfileDialog =
        document.createElement('esim-remove-profile-dialog');
    esimRemoveProfileDialog.iccid = iccid;
    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    await flushAsync();
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  async function getProfileForIccid(profiles, iccid) {
    for (const profile of profiles) {
      const properties = await profile.getProperties();
      if (properties.properties && properties.properties.iccid === iccid) {
        return profile;
      }
    }

    return null;
  }

  test('Remove esim profile', async function() {
    eSimManagerRemote.addEuiccForTest(2);
    init('1');

    await flushAsync();

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    let profiles = (await euicc.getProfileList()).profiles;
    let foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    const removeBtn = esimRemoveProfileDialog.$$('#remove');
    assertTrue(!!removeBtn);
    removeBtn.click();
    await flushAsync();
    foundProfile.resolveUninstallProfilePromise();
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertFalse(!!foundProfile);
  });

  test('Remove esim profile fails', async function() {
    eSimManagerRemote.addEuiccForTest(2);
    init('1');

    await flushAsync();

    assertTrue(esimRemoveProfileDialog.$$('#errorMessage').hidden);

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    let profiles = (await euicc.getProfileList()).profiles;

    let foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);
    foundProfile.setEsimOperationResultForTest(
        chromeos.cellularSetup.mojom.ESimOperationResult.kFailure);

    const removeBtn = esimRemoveProfileDialog.$$('#remove');
    assertTrue(!!removeBtn);
    assertFalse(removeBtn.disabled);
    removeBtn.click();
    await flushAsync();
    assertTrue(removeBtn.disabled);
    foundProfile.resolveUninstallProfilePromise();
    await flushAsync();
    assertFalse(removeBtn.disabled);

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);
    assertFalse(esimRemoveProfileDialog.$$('#errorMessage').hidden);
  });
});