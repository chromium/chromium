// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {setESimManagerRemoteForTesting} from 'chrome://resources/ash/common/cellular_setup/mojo_interface_provider.js';
import {MojoInterfaceProviderImpl} from 'chrome://resources/ash/common/network/mojo_interface_provider.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {ESimOperationResult} from 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeNetworkConfig} from 'chrome://webui-test/chromeos/fake_network_config_mojom.js';
import {FakeESimManagerRemote} from 'chrome://webui-test/cr_components/chromeos/cellular_setup/fake_esim_manager_remote.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('EsimRenameDialog', function() {
  const TEST_CELLULAR_GUID = 'cellular_guid';

  let esimRenameDialog;
  let eSimManagerRemote;
  let mojoApi_;

  setup(function() {
    eSimManagerRemote = new FakeESimManagerRemote();
    setESimManagerRemoteForTesting(eSimManagerRemote);

    mojoApi_ = new FakeNetworkConfig();
    MojoInterfaceProviderImpl.getInstance().remote_ = mojoApi_;
    mojoApi_.resetForTest();
    return flushAsync();
  });

  async function init() {
    esimRenameDialog = document.createElement('esim-rename-dialog');
    const response = await mojoApi_.getNetworkState(TEST_CELLULAR_GUID);
    esimRenameDialog.networkState = response.result;
    document.body.appendChild(esimRenameDialog);
    assertTrue(!!esimRenameDialog);
    await flushAsync();
    assertEquals(
        esimRenameDialog.shadowRoot.querySelector('#eSimprofileName')
            .shadowRoot.querySelector('input'),
        getDeepActiveElement());
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
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  function addEsimCellularNetwork(guid, iccid) {
    const cellular = OncMojo.getDefaultManagedProperties(
        NetworkType.kCellular, guid, 'profile' + iccid);
    cellular.typeProperties.cellular.iccid = iccid;
    cellular.typeProperties.cellular.eid = iccid + 'eid';
    mojoApi_.setManagedPropertiesForTest(cellular);
  }

  /**
   * @param {string} value The value of the input
   * @param {boolean} invalid If the input is invalid or not
   * @param {string} inputCount The length of value in string
   *     format, with 2 digits
   */
  function assertInput(value, invalid, valueLength) {
    const inputBox =
        esimRenameDialog.shadowRoot.querySelector('#eSimprofileName');
    const inputCount = esimRenameDialog.shadowRoot.querySelector('#inputCount');
    assertTrue(!!inputBox);
    assertTrue(!!inputCount);

    assertEquals(inputBox.value, value);
    assertEquals(inputBox.invalid, invalid);
    const characterCountText = esimRenameDialog.i18n(
        'eSimRenameProfileInputCharacterCount', valueLength, 20);
    assertEquals(inputCount.textContent.trim(), characterCountText);
    assertEquals(
        inputBox.ariaDescription,
        esimRenameDialog.i18n('eSimRenameProfileInputA11yLabel', 20));
  }

  test('Rename esim profile', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    await init();

    return flushAsync().then(async () => {
      const inputBox =
          esimRenameDialog.shadowRoot.querySelector('#eSimprofileName');
      assertTrue(!!inputBox);
      const profileName = inputBox.value;

      assertEquals(profileName, 'profile1');

      inputBox.value = 'new profile nickname';
      await flushAsync();

      const doneBtn = esimRenameDialog.shadowRoot.querySelector('#done');
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
    flush();

    return flushAsync().then(async () => {
      await flushAsync();
      const doneBtn = esimRenameDialog.shadowRoot.querySelector('#done');

      assertTrue(!!doneBtn);
      assertFalse(doneBtn.disabled);
      assertEquals(
          'block',
          window
              .getComputedStyle(
                  esimRenameDialog.shadowRoot.querySelector('#errorMessage'))
              .display);
    });
  });

  test('Rename esim profile fails', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    await init();

    return flushAsync().then(async () => {
      const inputBox =
          esimRenameDialog.shadowRoot.querySelector('#eSimprofileName');
      assertTrue(!!inputBox);
      const profileName = inputBox.value;

      assertEquals(profileName, 'profile1');

      assertEquals(
          'none',
          window
              .getComputedStyle(
                  esimRenameDialog.shadowRoot.querySelector('#errorMessage'))
              .display);

      const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
      const profile = (await euicc.getProfileList()).profiles[0];

      profile.setEsimOperationResultForTest(ESimOperationResult.kFailure);

      inputBox.value = 'new profile nickname';
      await flushAsync();

      const showErrorToastPromise =
          eventToPromise('show-error-toast', esimRenameDialog);

      const doneBtn = esimRenameDialog.shadowRoot.querySelector('#done');
      const cancelBtn = esimRenameDialog.shadowRoot.querySelector('#cancel');
      assertTrue(!!doneBtn);
      assertTrue(!!cancelBtn);
      assertFalse(doneBtn.disabled);
      assertFalse(cancelBtn.disabled);
      assertFalse(inputBox.disabled);
      doneBtn.click();
      await flushAsync();

      assertTrue(doneBtn.disabled);
      assertTrue(cancelBtn.disabled);
      assertTrue(inputBox.disabled);

      profile.resolveSetProfileNicknamePromise_();
      await flushAsync();
      assertFalse(doneBtn.disabled);
      assertFalse(cancelBtn.disabled);
      assertFalse(inputBox.disabled);

      const profileProperties = (await profile.getProperties()).properties;

      const showErrorToastEvent = await showErrorToastPromise;
      assertEquals(
          showErrorToastEvent.detail,
          esimRenameDialog.i18n('eSimRenameProfileDialogErrorToast'));
      assertNotEquals(
          convertString16ToJSString_(profileProperties.nickname),
          'new profile nickname');
    });
  });

  test('Warning message visibility', function() {
    const warningMessage =
        esimRenameDialog.shadowRoot.querySelector('#warningMessage');
    assertTrue(!!warningMessage);

    esimRenameDialog.showCellularDisconnectWarning = false;
    assertTrue(warningMessage.hidden);

    esimRenameDialog.showCellularDisconnectWarning = true;
    assertFalse(warningMessage.hidden);
  });

  test('Input is sanitized', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    await init();

    await flushAsync();
    const inputBox =
        esimRenameDialog.shadowRoot.querySelector('#eSimprofileName');
    assertTrue(!!inputBox);
    const profileName = inputBox.value;
    assertEquals(profileName, 'profile1');

    // Test empty name.
    inputBox.value = '';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name with no emojis, under character limit.
    inputBox.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with emojis, under character limit.
    inputBox.value = '1234😀5678901234🧟';
    assertInput(
        /*value=*/ '12345678901234', /*invalid=*/ false,
        /*valueLength=*/ '14');

    // Test name with only emojis, under character limit.
    inputBox.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name with no emojis, at character limit.
    inputBox.value = '12345678901234567890';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ false,
        /*valueLength=*/ '20');

    // Test name with emojis, at character limit.
    inputBox.value = '1234567890123456789🧟';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with only emojis, at character limit.
    inputBox.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Test name with no emojis, above character limit.
    inputBox.value = '123456789012345678901';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ true,
        /*valueLength=*/ '20');

    // Make sure input is not invalid once its value changes to a string below
    // the character limit. (Simulates the user pressing backspace once they've
    // reached the limit).
    inputBox.value = '1234567890123456789';
    assertInput(
        /*value=*/ '1234567890123456789', /*invalid=*/ false,
        /*valueLength=*/ '19');

    // Test name with emojis, above character limit.
    inputBox.value = '12345678901234567890🧟';
    assertInput(
        /*value=*/ '12345678901234567890', /*invalid=*/ false,
        /*valueLength=*/ '20');

    // Test name with only emojis, above character limit.
    inputBox.value = '😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀';
    assertInput(
        /*value=*/ '', /*invalid=*/ false, /*valueLength=*/ '00');

    // Set name with emojis, above character limit
    inputBox.value = '12345678901234567890🧟';
    const doneBtn = esimRenameDialog.shadowRoot.querySelector('#done');
    assertTrue(!!doneBtn);
    doneBtn.click();
    await flushAsync();

    const euicc = (await eSimManagerRemote.getAvailableEuiccs()).euiccs[0];
    const profile = (await euicc.getProfileList()).profiles[0];
    const profileProperties = (await profile.getProperties()).properties;

    assertEquals(
        convertString16ToJSString_(profileProperties.nickname),
        '12345678901234567890');
  });

  test('Done button is disabled when empty input', async function() {
    eSimManagerRemote.addEuiccForTest(1);
    addEsimCellularNetwork(TEST_CELLULAR_GUID, '1');
    await flushAsync();
    await init();

    const inputBox =
        esimRenameDialog.shadowRoot.querySelector('#eSimprofileName');
    const doneBtn = esimRenameDialog.shadowRoot.querySelector('#done');
    assertTrue(!!inputBox);
    assertTrue(!!doneBtn);

    inputBox.value = 'test';
    assertFalse(doneBtn.disabled);

    inputBox.value = '';
    assertTrue(doneBtn.disabled);

    inputBox.value = 'test2';
    assertFalse(doneBtn.disabled);
  });
});
