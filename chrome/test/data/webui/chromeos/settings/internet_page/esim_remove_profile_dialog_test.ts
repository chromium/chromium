// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {EsimRemoveProfileDialogElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {ESimManagerRemote, ESimOperationResult, ESimProfileInterface} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote, FakeProfile} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('EsimRemoveProfileDialog', () => {
  const TEST_CELLULAR_GUID = 'cellular_guid';

  let esimRemoveProfileDialog: EsimRemoveProfileDialogElement;
  let eSimManagerRemote: FakeESimManagerRemote;

  let mojoApi_: FakeNetworkConfig;

  setup(() => {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().setMojoServiceRemoteForTest(
        mojoApi_);

    mojoApi_.resetForTest();
  });

  async function init() {
    clearBody();
    esimRemoveProfileDialog =
        document.createElement('esim-remove-profile-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRemoveProfileDialog.networkState = response.result;
    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    // TODO(b/214301268): Add interactive test for cancel button focus.
    await flushTasks();
  }

  async function getProfileForIccid(
    profiles: ESimProfileInterface[],
    iccid: string): Promise<ESimProfileInterface|null> {
        for (const profile of profiles) {
          const properties = await profile.getProperties();
          if (properties.properties && properties.properties.iccid === iccid) {
            return profile;
          }
        }

        return null;
  }

  function addEsimCellularNetwork(guid: string, iccid: string): void {
    const cellular = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'profile' + iccid);
    cellular.typeProperties.cellular!.iccid = iccid;
    cellular.typeProperties.cellular!.eid = iccid + 'eid';
    mojoApi_.setManagedPropertiesForTest(cellular);
  }

  test('Remove esim profile', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushTasks();
    init();

    await flushTasks();

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    assertTrue(!!euicc);
    let profiles: ESimProfileInterface[] =
        (await euicc.getProfileList()).profiles;
    let foundProfile: ESimProfileInterface|null =
        await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    const removeBtn =
        esimRemoveProfileDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#remove');
    assertTrue(!!removeBtn);

    removeBtn.click();
    await flushTasks();
    (foundProfile as unknown as FakeProfile).resolveUninstallProfilePromise();
    await flushTasks();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertFalse(!!foundProfile);

    assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());
  });

  test('Remove esim profile fails', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushTasks();
    init();

    await flushTasks();
    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    assertTrue(!!euicc);
    let profiles: ESimProfileInterface[] =
        (await euicc.getProfileList()).profiles;

    let foundProfile: ESimProfileInterface|null =
        await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);
    (foundProfile as unknown as FakeProfile)
        .setEsimOperationResultForTest(ESimOperationResult.kFailure);

    const showErrorToastPromise =
        eventToPromise('show-error-toast', esimRemoveProfileDialog);

    const removeBtn =
        esimRemoveProfileDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#remove');
    assertTrue(!!removeBtn);

    removeBtn.click();
    await flushTasks();
    (foundProfile as unknown as FakeProfile).resolveUninstallProfilePromise();
    await flushTasks();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());

    const showErrorToastEvent = await showErrorToastPromise;
    assertEquals(
        showErrorToastEvent.detail,
        esimRemoveProfileDialog.i18n('eSimRemoveProfileDialogError'));
  });

  test('esimProfileRemote_ falsey, remove profile', async () => {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushTasks();

    clearBody();
    esimRemoveProfileDialog =
        document.createElement('esim-remove-profile-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRemoveProfileDialog.networkState = response.result;
    // Setting iccid to null will result in improper initialization.
    esimRemoveProfileDialog.set('networkState.typeState.cellular.iccid', null);

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    assertTrue(!!euicc);
    let profiles: ESimProfileInterface[] =
        (await euicc.getProfileList()).profiles;

    let foundProfile: ESimProfileInterface|null =
        await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    const showErrorToastPromise =
        eventToPromise('show-error-toast', esimRemoveProfileDialog);

    document.body.appendChild(esimRemoveProfileDialog);
    assertTrue(!!esimRemoveProfileDialog);
    await flushTasks();

    profiles = (await euicc.getProfileList()).profiles;
    foundProfile = await getProfileForIccid(profiles, '1');
    assertTrue(!!foundProfile);

    assertEquals(routes.INTERNET_NETWORKS, Router.getInstance().currentRoute);
    assertEquals(
        'type=Cellular', Router.getInstance().getQueryParameters().toString());

    const showErrorToastEvent = await showErrorToastPromise;
    assertEquals(
        showErrorToastEvent.detail,
        esimRemoveProfileDialog.i18n('eSimRemoveProfileDialogError'));
  });

  test('Warning message visibility', () => {
    const warningMessage =
        esimRemoveProfileDialog.shadowRoot!.querySelector<HTMLElement>(
            '#warningMessage');
    assertTrue(!!warningMessage);

    esimRemoveProfileDialog.showCellularDisconnectWarning = false;
    assertTrue(warningMessage.hidden);

    esimRemoveProfileDialog.showCellularDisconnectWarning = true;
    assertFalse(warningMessage.hidden);
  });
});
