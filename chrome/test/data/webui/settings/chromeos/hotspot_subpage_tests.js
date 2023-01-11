// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {CrosHotspotConfigInterface, CrosHotspotConfigObserverInterface, HotspotAllowStatus, HotspotConfig, HotspotControlResult, HotspotInfo, HotspotState, SetHotspotConfigResult, WiFiSecurityMode} from 'chrome://resources/mojo/chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('HotspotSubpageTest', function() {
  /** @type {HotspotSubpageElement} */
  let hotspotSubpage = null;

  /** @type {?CrosHotspotConfigInterface} */
  let hotspotConfig = null;

  /**
   * @type {!CrosHotspotConfigObserverInterface}
   */
  let hotspotConfigObserver;

  suiteSetup(function() {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  /**
   * @param {URLSearchParams=} opt_urlParams
   * @return {!Promise}
   */
  function init() {
    PolymerTest.clearBody();
    hotspotSubpage = document.createElement('settings-hotspot-subpage');
    document.body.appendChild(hotspotSubpage);
    flush();

    hotspotConfigObserver = {
      /** override */
      onHotspotInfoChanged() {
        hotspotConfig.getHotspotInfo().then(response => {
          hotspotSubpage.hotspotInfo = response.hotspotInfo;
        });
      },
    };
    hotspotConfig.addObserver(hotspotConfigObserver);
    hotspotConfig.setFakeHotspotInfo({
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      config: {
        autoDisable: true,
        ssid: 'test_ssid',
        passphrase: 'test_passphrase',
      },
    });

    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL);
    return flushAsync();
  }

  teardown(function() {
    hotspotConfig.reset();
    hotspotSubpage.remove();
    hotspotSubpage = null;
    Router.getInstance().resetRouteForTesting();
  });

  function flushAsync() {
    flush();
    return new Promise(resolve => setTimeout(resolve));
  }

  test('Toggle button state and a11y', async function() {
    await init();
    const enableHotspotToggle =
        hotspotSubpage.shadowRoot.querySelector('#enableHotspotToggle');
    assertTrue(!!enableHotspotToggle);
    assertFalse(enableHotspotToggle.checked);

    // Simulate clicking toggle to turn on hotspot and fail.
    hotspotConfig.setFakeEnableHotspotResult(
        HotspotControlResult.kNetworkSetupFailure);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be off.
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);

    // Simulate clicking toggle to turn on hotspot and succeed.
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be on this time.
    assertTrue(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    let a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSubpage.i18n('hotspotEnabledA11yLabel')));

    // Simulate clicking on toggle to turn off hotspot and succeed.
    a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig.setFakeDisableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushAsync();
    // Toggle should be off
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSubpage.i18n('hotspotDisabledA11yLabel')));

    // Simulate state becoming kEnabling.
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushAsync();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);

    // Simulate AllowStatus becoming kDisallowedByPolicy.
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushAsync();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
  });

  test('UI state test', async function() {
    await init();
    // Simulate hotspot state is disabled.
    const hotspotOnOffLabel =
        hotspotSubpage.shadowRoot.querySelector('#hotspotToggleText');
    const enableToggle =
        hotspotSubpage.shadowRoot.querySelector('#enableHotspotToggle');
    const hotspotNameElement =
        hotspotSubpage.shadowRoot.querySelector('#hotspotSSID');
    const connectedClientCount =
        hotspotSubpage.shadowRoot.querySelector('#connectedDeviceCount');

    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent.trim());
    assertEquals('test_ssid', hotspotNameElement.textContent.trim());
    assertEquals('0', connectedClientCount.textContent.trim());
    assertFalse(enableToggle.checked);

    // Simulate turning on hotspot.
    hotspotConfig.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    hotspotConfig.enableHotspot();
    await flushAsync();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOn'),
        hotspotOnOffLabel.textContent.trim());
    assertTrue(enableToggle.checked);

    // Simulate turning off hotspot.
    hotspotConfig.setFakeDisableHotspotResult(HotspotControlResult.kSuccess);
    hotspotConfig.disableHotspot();
    await flushAsync();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent.trim());
    assertFalse(enableToggle.checked);

    // Verify toggle is able to turn on/off by CrosHotspotConfig even when it is
    // disabled by policy.
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushAsync();
    // Toggle should be disabled.
    assertTrue(enableToggle.disabled);

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabled);
    await flushAsync();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOn'),
        hotspotOnOffLabel.textContent.trim());
    assertTrue(enableToggle.checked);

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);
    await flushAsync();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent.trim());
    assertFalse(enableToggle.checked);

    hotspotConfig.setFakeHotspotActiveClientCount(6);
    await flushAsync();
    assertEquals('6', connectedClientCount.textContent.trim());

    hotspotConfig.setFakeHotspotConfig({
      ssid: 'new_ssid',
    });
    await flushAsync();
    assertEquals('new_ssid', hotspotNameElement.textContent.trim());

    // Verifies UI with null hotspot config
    hotspotConfig.setFakeHotspotConfig(null);
    await flushAsync();
    assertEquals('', hotspotNameElement.textContent.trim());
  });

  test('Auto disable toggle', async function() {
    await init();
    let autoDisableToggle =
        hotspotSubpage.shadowRoot.querySelector('#hotspotAutoDisableToggle');
    assertTrue(!!autoDisableToggle);
    assertTrue(autoDisableToggle.checked);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    autoDisableToggle.click();
    await flushAsync();
    assertFalse(autoDisableToggle.checked);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kFailedInvalidConfiguration);
    autoDisableToggle.click();
    await flushAsync();
    assertFalse(autoDisableToggle.checked);

    // Verifies that the toggle should be hidden if the hotspot config is null.
    hotspotConfig.setFakeHotspotConfig(null);
    await flushAsync();
    autoDisableToggle =
        hotspotSubpage.shadowRoot.querySelector('#hotspotAutoDisableToggle');
    assertEquals(null, autoDisableToggle);
  });

  test('Hide configure button when hotspot config is null', async function() {
    await init();
    const configureButton =
        hotspotSubpage.shadowRoot.querySelector('#configureButton');
    assertTrue(!!configureButton, 'Hotspot configure button does not exist');
    assertFalse(configureButton.hidden);

    hotspotConfig.setFakeHotspotConfig(null);
    await flushAsync();
    assertTrue(configureButton.hidden);
  });

  test(
      'Click on configure button should fire show-hotspot-config-dialog event',
      async function() {
        await init();
        const configureButton =
            hotspotSubpage.shadowRoot.querySelector('#configureButton');
        assertTrue(
            !!configureButton, 'Hotspot configure button does not exist');
        assertFalse(configureButton.hidden);

        const showHotspotConfigDialogEvent =
            eventToPromise('show-hotspot-config-dialog', hotspotSubpage);
        configureButton.click();
        await Promise.all([showHotspotConfigDialogEvent, flushTasks()]);
      });
});