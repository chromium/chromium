// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {HotspotConfigDialogElement, Router, routes, WiFiSecurityType} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotState, SetHotspotConfigResult, WiFiBand, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {NetworkConfigInputElement} from 'chrome://resources/ash/common/network/network_config_input.js';
import {NetworkConfigSelectElement} from 'chrome://resources/ash/common/network/network_config_select.js';
import {NetworkPasswordInputElement} from 'chrome://resources/ash/common/network/network_password_input.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<hotspot-config-dialog>', () => {
  let hotspotConfigDialog: HotspotConfigDialogElement;
  let hotspotConfig: FakeHotspotConfig;

  suiteSetup(() => {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  teardown(() => {
    hotspotConfig.reset();
    hotspotConfigDialog.remove();
    Router.getInstance().resetRouteForTesting();
  });

  async function init() {
    const hotspotInfo = {
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      allowedWifiSecurityModes: [
        WiFiSecurityMode.kWpa2,
        WiFiSecurityMode.kWpa3,
        WiFiSecurityMode.kWpa2Wpa3,
      ],
      config: {
        autoDisable: true,
        security: WiFiSecurityMode.kWpa2,
        ssid: 'test_ssid',
        passphrase: 'test_passphrase',
        band: WiFiBand.kAutoChoose,
        bssidRandomization: true,
      },
      clientCount: 0,
    };
    hotspotConfigDialog = document.createElement('hotspot-config-dialog');
    hotspotConfig.setFakeHotspotInfo(hotspotInfo);
    const response = await hotspotConfig.getHotspotInfo();
    hotspotConfigDialog.hotspotInfo = response.hotspotInfo;
    document.body.appendChild(hotspotConfigDialog);
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
    await flushTasks();
  }

  test('Name validation and update hotspot SSID', async () => {
    await init();

    const hotspotNameInput =
        hotspotConfigDialog.shadowRoot!
            .querySelector<NetworkConfigInputElement>('#hotspotName');
    assertTrue(!!hotspotNameInput, 'Hotspot name input doesn\'t exist');
    assertEquals('test_ssid', hotspotNameInput.value);

    const hotspotNameInputInfo =
        hotspotConfigDialog.shadowRoot!.querySelector('#hotspotNameInputInfo');
    assertTrue(
        !!hotspotNameInputInfo, 'Hotspot name input info doesn\'t exist');

    const saveBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#saveButton');
    assertTrue(!!saveBtn);
    const cancelBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#cancelButton');
    assertTrue(!!cancelBtn);

    hotspotNameInput.value = '';
    assertTrue(hotspotNameInput.invalid);
    assertTrue(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);
    assertEquals(
        hotspotConfigDialog.i18n('hotspotConfigNameEmptyInfo'),
        hotspotNameInputInfo.textContent!.trim());
    assertTrue(hotspotNameInputInfo.classList.contains('error'));

    hotspotNameInput.value = 'new_ssid';
    assertFalse(hotspotNameInput.invalid);
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);
    assertEquals(
        hotspotConfigDialog.i18n('hotspotConfigNameInfo'),
        hotspotNameInputInfo.textContent!.trim());
    assertFalse(hotspotNameInputInfo.classList.contains('error'));

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    saveBtn.click();
    await flushTasks();

    const response = await hotspotConfig.getHotspotInfo();
    assertTrue(!!response.hotspotInfo.config);
    assertEquals('new_ssid', response.hotspotInfo.config.ssid);
  });

  test('Password validation and update hotspot password', async () => {
    await init();

    const hotspotPasswordInput =
        hotspotConfigDialog.shadowRoot!
            .querySelector<NetworkPasswordInputElement>('#hotspotPassword');
    assertTrue(!!hotspotPasswordInput, 'Hotspot password input doesn\'t exist');
    assertEquals('test_passphrase', hotspotPasswordInput.value);

    const saveBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#saveButton');
    assertTrue(!!saveBtn);
    const cancelBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#cancelButton');
    assertTrue(!!cancelBtn);

    const hotspotPasswordInputInfo =
        hotspotConfigDialog.shadowRoot!.querySelector(
            '#hotspotPasswordInputInfo');
    assertTrue(
        !!hotspotPasswordInputInfo,
        'Hotspot password input info doesn\'t exist');

    // Verifies that password has to be at least 8 chars long.
    hotspotPasswordInput.value = 'short';
    assertTrue(hotspotPasswordInput.invalid);
    assertTrue(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);
    assertTrue(hotspotPasswordInputInfo.classList.contains('error'));

    hotspotPasswordInput.value = 'validlong_password';
    assertFalse(hotspotPasswordInput.invalid);
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);
    assertEquals(
        hotspotConfigDialog.i18n('hotspotConfigPasswordInfo'),
        hotspotPasswordInputInfo.textContent!.trim());
    assertFalse(hotspotPasswordInputInfo.classList.contains('error'));

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    saveBtn.click();
    await flushTasks();

    const response = await hotspotConfig.getHotspotInfo();
    assertTrue(!!response.hotspotInfo.config);
    assertEquals('validlong_password', response.hotspotInfo.config.passphrase);
  });

  test('Security select should show and update hotspot security', async () => {
    await init();

    const hotspotSecuritySelect =
        hotspotConfigDialog.shadowRoot!
            .querySelector<NetworkConfigSelectElement>('#security');
    assertTrue(
        !!hotspotSecuritySelect, 'Hotspot security select doesn\'t exist');

    assertEquals(WiFiSecurityType.WPA2, hotspotSecuritySelect.value);
    assertEquals(3, hotspotSecuritySelect.items.length);
    assertEquals(WiFiSecurityType.WPA2, hotspotSecuritySelect.items[0]);
    assertEquals(WiFiSecurityType.WPA3, hotspotSecuritySelect.items[1]);
    assertEquals(WiFiSecurityType.WPA2WPA3, hotspotSecuritySelect.items[2]);
    hotspotSecuritySelect.value = WiFiSecurityType.WPA3;

    const saveBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#saveButton');
    assertTrue(!!saveBtn);
    const cancelBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#cancelButton');
    assertTrue(!!cancelBtn);
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    saveBtn.click();
    await flushTasks();

    const response = await hotspotConfig.getHotspotInfo();
    assertTrue(!!response.hotspotInfo.config);
    assertEquals(WiFiSecurityMode.kWpa3, response.hotspotInfo.config.security);
  });

  test(
      'Hotspot bssid randomization toggle should show and update bssid ' +
          'randomization',
      async () => {
        await init();

        const hotspotBssidToggle =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLInputElement>(
                '#hotspotBssidToggle');
        assertTrue(
            !!hotspotBssidToggle,
            'Hotspot Bssid randomization toggle doesn\'t exist');
        assertTrue(hotspotBssidToggle.checked);

        hotspotBssidToggle.click();
        assertFalse(hotspotBssidToggle.checked);

        const saveBtn =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#saveButton');
        assertTrue(!!saveBtn);
        const cancelBtn =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#cancelButton');
        assertTrue(!!cancelBtn);
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kSuccess);
        saveBtn.click();
        await flushTasks();

        const response = await hotspotConfig.getHotspotInfo();
        assertTrue(!!response.hotspotInfo.config);
        assertFalse(response.hotspotInfo.config.bssidRandomization);
      });

  test(
      'Hotspot extend compatibility toggle should show and update ' +
          'compatibility',
      async () => {
        await init();

        const hotspotCompatibilityToggle =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLInputElement>(
                '#hotspotCompatibilityToggle');
        assertTrue(
            !!hotspotCompatibilityToggle,
            'Hotspot extend compatibility toggle doesn\'t exist');
        assertFalse(hotspotCompatibilityToggle.checked);

        hotspotCompatibilityToggle.click();
        assertTrue(hotspotCompatibilityToggle.checked);

        const saveBtn =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#saveButton');
        assertTrue(!!saveBtn);
        const cancelBtn =
            hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
                '#cancelButton');
        assertTrue(!!cancelBtn);
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kSuccess);
        saveBtn.click();
        await flushTasks();

        const response = await hotspotConfig.getHotspotInfo();
        assertTrue(!!response.hotspotInfo.config);
        assertEquals(WiFiBand.k2_4GHz, response.hotspotInfo.config.band);
      });

  test('When save config fails, it should show error message', async () => {
    await init();

    const hotspotNameInput =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#hotspotName');
    assertTrue(!!hotspotNameInput, 'Hotspot name input doesn\'t exist');
    assertEquals('test_ssid', hotspotNameInput.value);
    hotspotNameInput.value = 'new_ssid';

    const saveBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#saveButton');
    assertTrue(!!saveBtn);
    const cancelBtn =
        hotspotConfigDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#cancelButton');
    assertTrue(!!cancelBtn);
    let errorMessageElement =
        hotspotConfigDialog.shadowRoot!.querySelector('#errorMessage');
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);
    assertNull(errorMessageElement);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kFailedInvalidConfiguration);
    saveBtn.click();
    await flushTasks();
    errorMessageElement =
        hotspotConfigDialog.shadowRoot!.querySelector('#errorMessage');
    assertTrue(!!errorMessageElement, 'Hotspot error message doesn\'t show');
    assertEquals(
        hotspotConfigDialog.i18n(
            'hotspotConfigInvalidConfigurationErrorMessage'),
        errorMessageElement.textContent!.trim());

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kFailedNotLogin);
    saveBtn.click();
    await flushTasks();
    errorMessageElement =
        hotspotConfigDialog.shadowRoot!.querySelector('#errorMessage');
    assertTrue(!!errorMessageElement, 'Hotspot error message doesn\'t show');
    assertEquals(
        hotspotConfigDialog.i18n('hotspotConfigNotLoginErrorMessage'),
        errorMessageElement.textContent!.trim());

    cancelBtn.click();
    await flushTasks();

    const response = await hotspotConfig.getHotspotInfo();
    assertTrue(!!response.hotspotInfo.config);
    assertEquals('test_ssid', response.hotspotInfo.config.ssid);
  });
});
