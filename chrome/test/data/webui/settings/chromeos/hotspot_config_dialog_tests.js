// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {HotspotAllowStatus, HotspotState, SetHotspotConfigResult, WiFiBand, WiFiSecurityMode} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('HotspotConfigDialog', function() {
  /** @type {?HotspotConfigDialogElement} */
  let hotspotConfigDialog = null;

  /** @type {?CrosHotspotConfigInterface} */
  let hotspotConfig = null;

  /**
   * Security types for Hotspot Wi-Fi.
   * @enum {string}
   */
  const WiFiSecurityType = {
    WPA2: 'WPA2',
    WPA3: 'WPA3',
    WPA2WPA3: 'WPA2WPA3',
  };

  suiteSetup(function() {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  teardown(function() {
    PolymerTest.clearBody();
    hotspotConfig.reset();
    hotspotConfigDialog.remove();
    hotspotConfigDialog = null;
    Router.getInstance().resetRouteForTesting();
  });

  async function init() {
    PolymerTest.clearBody();
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
    };
    hotspotConfigDialog = document.createElement('hotspot-config-dialog');
    hotspotConfig.setFakeHotspotInfo(hotspotInfo);
    const response = await hotspotConfig.getHotspotInfo();
    hotspotConfigDialog.hotspotInfo = response.hotspotInfo;
    document.body.appendChild(hotspotConfigDialog);
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
    await flushAsync();
  }

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Name input should show and update hotspot SSID', async function() {
    await init();

    const hotspotNameInput =
        hotspotConfigDialog.shadowRoot.querySelector('#hotspotName');
    assertTrue(!!hotspotNameInput, 'Hotspot name input doesn\'t exist');
    assertEquals('test_ssid', hotspotNameInput.value);
    hotspotNameInput.value = 'new_ssid';

    const saveBtn = hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
    const cancelBtn =
        hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    saveBtn.click();
    await flushAsync();

    const response = await hotspotConfig.getHotspotInfo();
    assertEquals('new_ssid', response.hotspotInfo.config.ssid);
  });

  test('Password validation and update hotspot password', async function() {
    await init();

    const hotspotPasswordInput =
        hotspotConfigDialog.shadowRoot.querySelector('#hotspotPassword');
    assertTrue(!!hotspotPasswordInput, 'Hotspot password input doesn\'t exist');
    assertEquals('test_passphrase', hotspotPasswordInput.value);

    const saveBtn = hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
    const cancelBtn =
        hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');

    // Verifies that password has to be at least 8 chars long.
    hotspotPasswordInput.value = 'short';
    assertTrue(hotspotPasswordInput.invalid);
    assertTrue(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);

    hotspotPasswordInput.value = 'validlong_password';
    assertFalse(hotspotPasswordInput.invalid);
    assertFalse(saveBtn.disabled);
    assertFalse(cancelBtn.disabled);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    saveBtn.click();
    await flushAsync();

    const response = await hotspotConfig.getHotspotInfo();
    assertEquals('validlong_password', response.hotspotInfo.config.passphrase);
  });

  test(
      'Security select should show and update hotspot security',
      async function() {
        await init();

        const hotspotSecuritySelect =
            hotspotConfigDialog.shadowRoot.querySelector('#security');
        assertTrue(
            !!hotspotSecuritySelect, 'Hotspot security select doesn\'t exist');

        assertEquals(WiFiSecurityType.WPA2, hotspotSecuritySelect.value);
        assertEquals(3, hotspotSecuritySelect.items.length);
        assertEquals(WiFiSecurityType.WPA2, hotspotSecuritySelect.items[0]);
        assertEquals(WiFiSecurityType.WPA3, hotspotSecuritySelect.items[1]);
        assertEquals(WiFiSecurityType.WPA2WPA3, hotspotSecuritySelect.items[2]);
        hotspotSecuritySelect.value = WiFiSecurityType.WPA3;

        const saveBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
        const cancelBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kSuccess);
        saveBtn.click();
        await flushAsync();

        const response = await hotspotConfig.getHotspotInfo();
        assertEquals(
            WiFiSecurityMode.kWpa3, response.hotspotInfo.config.security);
      });

  test(
      'Hotspot bssid randomization toggle should show and update bssid ' +
          'randomization',
      async function() {
        await init();

        const hotspotBssidToggle =
            hotspotConfigDialog.shadowRoot.querySelector('#hotspotBssidToggle');
        assertTrue(
            !!hotspotBssidToggle,
            'Hotspot Bssid randomization toggle doesn\'t exist');
        assertTrue(hotspotBssidToggle.checked);

        hotspotBssidToggle.click();
        assertFalse(hotspotBssidToggle.checked);

        const saveBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
        const cancelBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kSuccess);
        saveBtn.click();
        await flushAsync();

        const response = await hotspotConfig.getHotspotInfo();
        assertFalse(response.hotspotInfo.config.bssidRandomization);
      });

  test(
      'Hotspot extend compatibility toggle should show and update ' +
          'compatibility',
      async function() {
        await init();

        const hotspotCompatibilityToggle =
            hotspotConfigDialog.shadowRoot.querySelector(
                '#hotspotCompatibilityToggle');
        assertTrue(
            !!hotspotCompatibilityToggle,
            'Hotspot extend compatibility toggle doesn\'t exist');
        assertFalse(hotspotCompatibilityToggle.checked);

        hotspotCompatibilityToggle.click();
        assertTrue(hotspotCompatibilityToggle.checked);

        const saveBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
        const cancelBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kSuccess);
        saveBtn.click();
        await flushAsync();

        const response = await hotspotConfig.getHotspotInfo();
        assertEquals(WiFiBand.k2_4GHz, response.hotspotInfo.config.band);
      });

  test(
      'When save config fails, it should show error message', async function() {
        await init();

        const hotspotNameInput =
            hotspotConfigDialog.shadowRoot.querySelector('#hotspotName');
        assertTrue(!!hotspotNameInput, 'Hotspot name input doesn\'t exist');
        assertEquals('test_ssid', hotspotNameInput.value);
        hotspotNameInput.value = 'new_ssid';

        const saveBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#saveButton');
        const cancelBtn =
            hotspotConfigDialog.shadowRoot.querySelector('#cancelButton');
        let errorMessageElement =
            hotspotConfigDialog.shadowRoot.querySelector('#errorMessage');
        assertFalse(saveBtn.disabled);
        assertFalse(cancelBtn.disabled);
        assertEquals(null, errorMessageElement);

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kFailedInvalidConfiguration);
        saveBtn.click();
        await flushAsync();
        errorMessageElement =
            hotspotConfigDialog.shadowRoot.querySelector('#errorMessage');
        assertTrue(
            !!errorMessageElement, 'Hotspot error message doesn\'t show');
        assertEquals(
            hotspotConfigDialog.i18n(
                'hotspotConfigInvalidConfigurationErrorMessage'),
            errorMessageElement.textContent.trim());

        hotspotConfig.setFakeSetHotspotConfigResult(
            SetHotspotConfigResult.kFailedNotLogin);
        saveBtn.click();
        await flushAsync();
        errorMessageElement =
            hotspotConfigDialog.shadowRoot.querySelector('#errorMessage');
        assertTrue(
            !!errorMessageElement, 'Hotspot error message doesn\'t show');
        assertEquals(
            hotspotConfigDialog.i18n('hotspotConfigNotLoginErrorMessage'),
            errorMessageElement.textContent.trim());

        cancelBtn.click();
        await flushAsync();

        const response = await hotspotConfig.getHotspotInfo();
        assertEquals('test_ssid', response.hotspotInfo.config.ssid);
      });
});
