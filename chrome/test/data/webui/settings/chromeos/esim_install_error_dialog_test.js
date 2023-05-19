// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';

suite('EsimInstallErrorDialog', function() {
  let esimInstallErrorDialog;
  let eSimManagerRemote;
  let doneButton;

  async function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(async function() {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);
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

    doneButton = esimInstallErrorDialog.shadowRoot.querySelector('#done');
    assertTrue(!!doneButton);
  });

  suite('Confirmation code error', function() {
    let input;

    setup(async function() {
      esimInstallErrorDialog.errorCode =
          ProfileInstallResult.kErrorNeedsConfirmationCode;
      await flushAsync();

      assertTrue(!!esimInstallErrorDialog.shadowRoot.querySelector(
          '#confirmationCodeErrorContainer'));
      assertTrue(esimInstallErrorDialog.$.genericErrorContainer.hidden);
      assertFalse(esimInstallErrorDialog.$.cancel.hidden);

      input =
          esimInstallErrorDialog.shadowRoot.querySelector('#confirmationCode');
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

      assertEquals(profileProperties.state, ProfileState.kActive);
      assertFalse(esimInstallErrorDialog.$.installErrorDialog.open);
    });

    test('Install profile unsuccessful', async function() {
      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];
      profile.setProfileInstallResultForTest(ProfileInstallResult.kFailure);

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
      assertEquals(profileProperties.state, ProfileState.kPending);
      assertTrue(esimInstallErrorDialog.$.installErrorDialog.open);

      input.value = 'CONFIRMATION_COD';
      assertFalse(input.invalid);
    });
  });

  suite('Generic error', function() {
    setup(async function() {
      esimInstallErrorDialog.errorCode = ProfileInstallResult.kFailure;
      await flushAsync();

      assertFalse(!!esimInstallErrorDialog.shadowRoot.querySelector(
          '#confirmationCodeErrorContainer'));
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
