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
// #import {eventToPromise} from 'chrome://test/test_util.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

suite('EsimRemoveProfileDialog', function() {
  const TEST_CELLULAR_GUID = 'cellular_guid';

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
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRemoveProfileDialog.networkState = response.result;
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

  function addEsimCellularNetwork(guid, iccid) {
    const cellular = OncMojo.getDefaultManagedProperties(
        chromeos.networkConfig.mojom.NetworkType.kCellular, guid,
        'profile' + iccid);
    cellular.typeProperties.cellular.iccid = iccid;
    cellular.typeProperties.cellular.eid = iccid + 'eid';
    mojoApi_.setManagedPropertiesForTest(cellular);
  }

  test('Remove esim profile', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    init();

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

    assertEquals(
        settings.routes.INTERNET_NETWORKS,
        settings.Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular',
        settings.Router.getInstance().getQueryParameters().toString());
  });

  test('Remove esim profile fails', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    init();

    await flushAsync();
    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    let profiles = (await euicc.getProfileList()).profiles;

    let foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);
    foundProfile.setEsimOperationResultForTest(
        chromeos.cellularSetup.mojom.ESimOperationResult.kFailure);

    const showErrorToastPromise =
        test_util.eventToPromise('show-error-toast', esimRemoveProfileDialog);

    const removeBtn = esimRemoveProfileDialog.$$('#remove');
    assertTrue(!!removeBtn);

    removeBtn.click();
    await flushAsync();
    foundProfile.resolveUninstallProfilePromise();
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(
        settings.routes.INTERNET_NETWORKS,
        settings.Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular',
        settings.Router.getInstance().getQueryParameters().toString());

    const showErrorToastEvent = await showErrorToastPromise;
    assertEquals(
        showErrorToastEvent.detail,
        esimRemoveProfileDialog.i18n('eSimRemoveProfileDialogError'));
  });

  test('esimProfileRemote_ falsey, remove profile', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();

    esimRemoveProfileDialog =
        document.createElement('esim-remove-profile-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRemoveProfileDialog.networkState = response.result;
    // Setting iccid to null wil result in improper initialization.
    esimRemoveProfileDialog.networkState.typeState.cellular.iccid = null;

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    let profiles = (await euicc.getProfileList()).profiles;

    let foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    const showErrorToastPromise =
        test_util.eventToPromise('show-error-toast', esimRemoveProfileDialog);

    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(
        settings.routes.INTERNET_NETWORKS,
        settings.Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular',
        settings.Router.getInstance().getQueryParameters().toString());

    const showErrorToastEvent = await showErrorToastPromise;
    assertEquals(
        showErrorToastEvent.detail,
        esimRemoveProfileDialog.i18n('eSimRemoveProfileDialogError'));
  });

  test('Warning message visibility', function() {
    const warningMessage = esimRemoveProfileDialog.$$('#warningMessage');
    assertTrue(!!warningMessage);

    esimRemoveProfileDialog.showCellularDisconnectWarning = false;
    assertTrue(warningMessage.hidden);

    esimRemoveProfileDialog.showCellularDisconnectWarning = true;
    assertFalse(warningMessage.hidden);
  });
});