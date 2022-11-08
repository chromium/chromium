// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ESimOperationResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('EsimRemoveProfileDialog', function() {
  const TEST_CELLULAR_GUID = 'cellular_guid';

  let esimRemoveProfileDialog;
  let eSimManagerRemote;
  let mojoApi_;

  setup(function() {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();
  });

  async function init(iccid) {
    esimRemoveProfileDialog =
        document.createElement('esim-remove-profile-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRemoveProfileDialog.networkState = response.result;
    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    // TODO(b/214301268): Add interactive test for cancel button focus.
    await flushAsync();
  }

  function flushAsync() {
    flush();
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
        NetworkType.kCellular, guid, 'profile' + iccid);
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

    const removeBtn =
        esimRemoveProfileDialog.shadowRoot.querySelector('#remove');
    assertTrue(!!removeBtn);
    removeBtn.click();
    await flushAsync();
    foundProfile.resolveUninstallProfilePromise();
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertFalse(!!foundProfile);

    assertEquals(
        routes.INTERNET_NETWORKS, Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());
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
    foundProfile.setEsimOperationResultForTest(ESimOperationResult.kFailure);

    const showErrorToastPromise =
        eventToPromise('show-error-toast', esimRemoveProfileDialog);

    const removeBtn =
        esimRemoveProfileDialog.shadowRoot.querySelector('#remove');
    assertTrue(!!removeBtn);

    removeBtn.click();
    await flushAsync();
    foundProfile.resolveUninstallProfilePromise();
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(
        routes.INTERNET_NETWORKS, Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());

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
        eventToPromise('show-error-toast', esimRemoveProfileDialog);

    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    await flushAsync();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(
        routes.INTERNET_NETWORKS, Router.getInstance().getCurrentRoute());
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());

    const showErrorToastEvent = await showErrorToastPromise;
    assertEquals(
        showErrorToastEvent.detail,
        esimRemoveProfileDialog.i18n('eSimRemoveProfileDialogError'));
  });

  test('Warning message visibility', function() {
    const warningMessage =
        esimRemoveProfileDialog.shadowRoot.querySelector('#warningMessage');
    assertTrue(!!warningMessage);

    esimRemoveProfileDialog.showCellularDisconnectWarning = false;
    assertTrue(warningMessage.hidden);

    esimRemoveProfileDialog.showCellularDisconnectWarning = true;
    assertFalse(warningMessage.hidden);
  });
});
