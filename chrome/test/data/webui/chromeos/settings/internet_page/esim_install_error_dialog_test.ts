// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {EsimInstallErrorDialogElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement} from 'chrome://os-settings/os_settings.js';
import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {ESimManagerRemote, ProfileInstallResult, ProfileState} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeESimManagerRemote, FakeProfile} from 'chrome://webui-test/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<esim-install-error-dialog>', () => {
  let esimInstallErrorDialog: EsimInstallErrorDialogElement;
  let eSimManagerRemote: FakeESimManagerRemote;
  let doneButton: HTMLButtonElement;

  setup(async () => {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(
        eSimManagerRemote as unknown as ESimManagerRemote);
    eSimManagerRemote.addEuiccForTest(1);
    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    assertTrue(!!euicc);
    const profile = (await euicc.getProfileList()).profiles[0];
    assertTrue(!!profile);

    esimInstallErrorDialog =
        document.createElement('esim-install-error-dialog');
    esimInstallErrorDialog.profile = profile;
    document.body.appendChild(esimInstallErrorDialog);
    assertTrue(!!esimInstallErrorDialog);

    await flushTasks();

    const button =
        esimInstallErrorDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#done');
    assertTrue(!!button);
    doneButton = button;
  });

  teardown(() => {
    esimInstallErrorDialog.remove();
  });

  suite('Confirmation code error', () => {
    let input: CrInputElement;

    setup(async () => {
      esimInstallErrorDialog.errorCode =
          ProfileInstallResult.kErrorNeedsConfirmationCode;
      await flushTasks();

      assertTrue(!!esimInstallErrorDialog.shadowRoot!.querySelector(
          '#confirmationCodeErrorContainer'));

      const genericErrorContainer =
          esimInstallErrorDialog.shadowRoot!.querySelector<HTMLElement>(
              '#genericErrorContainer');
      const cancel =
          esimInstallErrorDialog.shadowRoot!.querySelector<HTMLElement>(
              '#cancel');
      assertTrue(!!genericErrorContainer);
      assertTrue(!!cancel);
      assertTrue(genericErrorContainer.hidden);
      assertFalse(cancel.hidden);

      const crInput =
          esimInstallErrorDialog.shadowRoot!.querySelector<CrInputElement>(
              '#confirmationCode');
      assertTrue(!!crInput);
      input = crInput;
      assertTrue(doneButton.disabled);
    });

    teardown(() => {
      esimInstallErrorDialog.remove();
    });

    test('Install profile successful', async () => {
      input.value = 'CONFIRMATION_CODE';
      assertFalse(doneButton.disabled);

      doneButton.click();

      assertTrue(input.disabled);
      assertTrue(doneButton.disabled);

      await flushTasks();

      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      assertTrue(!!euicc);
      const profile = (await euicc.getProfileList()).profiles[0];
      assertTrue(!!profile);
      const profileProperties = (await profile.getProperties()).properties;

      assertEquals(ProfileState.kActive, profileProperties.state);
      assertFalse(esimInstallErrorDialog.$.installErrorDialog.open);
    });

    test('Install profile unsuccessful', async () => {
      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      assertTrue(!!euicc);
      const profile =
          (await euicc.getProfileList()).profiles[0] as unknown as FakeProfile;
      assertTrue(!!profile);
      profile.setProfileInstallResultForTest(ProfileInstallResult.kFailure);

      input.value = 'CONFIRMATION_CODE';
      assertFalse(doneButton.disabled);

      doneButton.click();

      assertTrue(input.disabled);
      assertTrue(doneButton.disabled);

      await flushTasks();

      assertTrue(input.invalid);
      assertFalse(input.disabled);
      assertFalse(doneButton.disabled);

      const profileProperties = (await profile.getProperties()).properties;
      assertEquals(ProfileState.kPending, profileProperties.state);
      assertTrue(esimInstallErrorDialog.$.installErrorDialog.open);

      input.value = 'CONFIRMATION_COD';
      assertFalse(input.invalid);
    });
  });

  suite('Generic error', () => {
    setup(async () => {
      esimInstallErrorDialog.errorCode = ProfileInstallResult.kFailure;
      await flushTasks();

      assertNull(esimInstallErrorDialog.shadowRoot!.querySelector(
          '#confirmationCodeErrorContainer'));

      const genericErrorContainer =
          esimInstallErrorDialog.shadowRoot!.querySelector<HTMLElement>(
              '#genericErrorContainer');
      const cancel =
          esimInstallErrorDialog.shadowRoot!.querySelector<HTMLElement>(
              '#cancel');
      assertTrue(!!genericErrorContainer);
      assertTrue(!!cancel);

      assertFalse(genericErrorContainer.hidden);
      assertTrue(cancel.hidden);
    });

    teardown(() => {
      esimInstallErrorDialog.remove();
    });

    test('Done button closes dialog', async () => {
      assertTrue(esimInstallErrorDialog.$.installErrorDialog.open);
      assertFalse(doneButton.disabled);

      doneButton.click();
      await flushTasks();
      assertFalse(esimInstallErrorDialog.$.installErrorDialog.open);
    });
  });
});
