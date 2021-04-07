// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {FakeNetworkConfig} from 'chrome://test/chromeos/fake_network_config_mojom.m.js';
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// clang-format on

suite('EsimRenameDialog', function() {
  const TEST_CELLULAR_GUID = 'cellular_guid';

  let esimRenameDialog;
  let eSimManagerRemote;
  let mojoApi_;

  setup(function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);

    mojoApi_ = new FakeNetworkConfig();
    network_config.MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();
    return flushAsync();
  });

  async function init() {
    esimRenameDialog = document.createElement('esim-rename-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRenameDialog.networkState = response.result;
    document.body.appendChild(esimRenameDialog);
    assertTrue(!!esimRenameDialog);
    Polymer.dom.flush();
  }

  /**
   * Converts a mojoBase.mojom.String16 to a JavaScript String.
   * @param {?mojoBase.mojom.String16} str
   * @return {string}
   */
  function convertString16ToJSString_(str) {
    return str.data.map(ch => String.fromCodePoint(ch)).join('');
  }

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function addEsimCellularNetwork(guid, iccid) {
    const cellular = OncMojo.getDefaultManagedProperties(
        chromeos.networkConfig.mojom.NetworkType.kCellular, guid,
        'profile' + iccid);
    cellular.typeProperties.cellular.iccid = iccid;
    cellular.typeProperties.cellular.eid = iccid + 'eid';
    mojoApi_.setManagedPropertiesForTest(cellular);
  }

  test('Rename esim profile', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    init();

    return flushAsync().then(async () => {
      const inputBox = esimRenameDialog.$$('#eSimprofileName');
      assertTrue(!!inputBox);
      const profileName = inputBox.value;

      assertEquals(profileName, 'profile1');

      inputBox.value = 'new profile nickname';
      await flushAsync();

      const doneBtn = esimRenameDialog.$$('#done');
      assertTrue(!!doneBtn);
      doneBtn.click();
      await flushAsync();

      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];
      const profileProperties = (await profile.getProperties()).properties;

      assertEquals(
          convertString16ToJSString_(profileProperties.nickname),
          'new profile nickname');
    });
  });

  test('esimProfileRemote_ falsey, show error', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();

    esimRenameDialog = document.createElement('esim-rename-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRenameDialog.networkState = response.result;
    // Setting iccid to null wil result in improper initialization.
    esimRenameDialog.networkState.typeState.cellular.iccid = null;
    document.body.appendChild(esimRenameDialog);
    assertTrue(!!esimRenameDialog);
    Polymer.dom.flush();

    return flushAsync().then(async () => {
      await flushAsync();
      const doneBtn = esimRenameDialog.$$('#done');

      assertTrue(!!doneBtn);
      assertFalse(doneBtn.disabled);
      assertEquals(
          'block',
          window.getComputedStyle(esimRenameDialog.$$('#errorMessage'))
              .display);
    });
  });

  test('Rename esim profile fails', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    init();

    return flushAsync().then(async () => {
      const inputBox = esimRenameDialog.$$('#eSimprofileName');
      assertTrue(!!inputBox);
      const profileName = inputBox.value;

      assertEquals(profileName, 'profile1');

      assertEquals(
          'none',
          window.getComputedStyle(esimRenameDialog.$$('#errorMessage'))
              .display);

      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];

      profile.setEsimOperationResultForTest(
          chromeos.cellularSetup.mojom.ESimOperationResult.kFailure);

      inputBox.value = 'new profile nickname';
      await flushAsync();

      const doneBtn = esimRenameDialog.$$('#done');
      assertTrue(!!doneBtn);
      assertFalse(doneBtn.disabled);
      doneBtn.click();
      await flushAsync();
      assertTrue(doneBtn.disabled);

      profile.resolveSetProfileNicknamePromise_();
      await flushAsync();
      assertFalse(doneBtn.disabled);

      const profileProperties = (await profile.getProperties()).properties;

      assertEquals(
          'block',
          window.getComputedStyle(esimRenameDialog.$$('#errorMessage'))
              .display);
      assertNotEquals(
          convertString16ToJSString_(profileProperties.nickname),
          'new profile nickname');
    });
  });

  test('Warning message visibility', function() {
    const warningMessage = esimRenameDialog.$$('#warningMessage');
    assertTrue(!!warningMessage);

    esimRenameDialog.showCellularDisconnectWarning = false;
    assertTrue(warningMessage.hidden);

    esimRenameDialog.showCellularDisconnectWarning = true;
    assertFalse(warningMessage.hidden);
  });
});