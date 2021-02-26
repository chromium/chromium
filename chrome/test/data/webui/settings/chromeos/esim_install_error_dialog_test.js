// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {setESimManagerRemoteForTesting} from 'chrome://resources/cr_components/chromeos/cellular_setup/mojo_interface_provider.m.js';
// #import {FakeESimManagerRemote} from 'chrome://test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertEquals, assertTrue} from '../../chai_assert.js';
// clang-format on

suite('EsimInstallErrorDialog', function() {
  let esimInstallErrorDialog;
  let eSimManagerRemote;
  let doneButton;

  async function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(async function() {
    eSimManagerRemote = new cellular_setup.FakeESimManagerRemote();
    cellular_setup.setESimManagerRemoteForTesting(eSimManagerRemote);
    eSimManagerRemote.addEuiccForTest(1);
    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    const profile = (await euicc.getProfileList()).profiles[0];

    await flushAsync();

    esimInstallErrorDialog =
        document.createElement('esim-install-error-dialog');
    esimInstallErrorDialog.profile = profile;
    document.body.appendChild(esimInstallErrorDialog);
    assertTrue(!!esimInstallErrorDialog);

    await flushAsync();

    doneButton = esimInstallErrorDialog.$$('#done');
    assertTrue(!!doneButton);
  });

  suite('Confirmation code error', function() {
    let input;

    setup(async function() {
      esimInstallErrorDialog.errorCode =
          chromeos.cellularSetup.mojom.ProfileInstallResult
              .kErrorNeedsConfirmationCode;
      await flushAsync();

      assertTrue(
          !!esimInstallErrorDialog.$$('#confirmationCodeErrorContainer'));
      assertTrue(esimInstallErrorDialog.$.genericErrorContainer.hidden);
      assertFalse(esimInstallErrorDialog.$.cancel.hidden);

      input = esimInstallErrorDialog.$$('#confirmationCode');
      assertTrue(!!input);
      assertTrue(doneButton.disabled);
    });

    test('Install profile successful', async function() {
      input.value = 'CONFIRMATION_CODE';
      assertFalse(doneButton.disabled);

      doneButton.click();

      assertTrue(input.disabled);
      assertTrue(doneButton.disabled);

      await flushAsync();

      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];
      const profileProperties = (await profile.getProperties()).properties;

      assertEquals(
          profileProperties.state,
          chromeos.cellularSetup.mojom.ProfileState.kActive);
      assertFalse(esimInstallErrorDialog.$.installErrorDialog.open);
    });

    test('Install profile unsuccessful', async function() {
      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];
      profile.setProfileInstallResultForTest(
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure);

      input.value = 'CONFIRMATION_CODE';
      assertFalse(doneButton.disabled);

      doneButton.click();

      assertTrue(input.disabled);
      assertTrue(doneButton.disabled);

      await flushAsync();

      assertTrue(input.invalid);
      assertFalse(input.disabled);
      assertFalse(doneButton.disabled);

      const profileProperties = (await profile.getProperties()).properties;
      assertEquals(
          profileProperties.state,
          chromeos.cellularSetup.mojom.ProfileState.kPending);
      assertTrue(esimInstallErrorDialog.$.installErrorDialog.open);

      input.value = 'CONFIRMATION_COD';
      assertFalse(input.invalid);
    });
  });

  suite('Generic error', function() {
    setup(async function() {
      esimInstallErrorDialog.errorCode =
          chromeos.cellularSetup.mojom.ProfileInstallResult.kFailure;
      await flushAsync();

      assertFalse(
          !!esimInstallErrorDialog.$$('#confirmationCodeErrorContainer'));
      assertFalse(esimInstallErrorDialog.$.genericErrorContainer.hidden);
      assertTrue(esimInstallErrorDialog.$.cancel.hidden);
    });

    test('Done button closes dialog', async function() {
      assertTrue(esimInstallErrorDialog.$.installErrorDialog.open);
      assertFalse(doneButton.disabled);

      doneButton.click();
      await flushAsync();
      assertFalse(esimInstallErrorDialog.$.installErrorDialog.open);
    });
  });
});