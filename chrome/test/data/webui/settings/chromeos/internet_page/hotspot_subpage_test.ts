// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';
import 'chrome://os-settings/lazy_load.js';

import {SettingsHotspotSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrButtonElement, CrToggleElement, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {setHotspotConfigForTesting} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.js';
import {CrosHotspotConfigObserverInterface, CrosHotspotConfigObserverRemote, HotspotAllowStatus, HotspotConfig, HotspotControlResult, HotspotInfo, HotspotState, SetHotspotConfigResult} from 'chrome://resources/ash/common/hotspot/cros_hotspot_config.mojom-webui.js';
import {FakeHotspotConfig} from 'chrome://resources/ash/common/hotspot/fake_hotspot_config.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('<settings-hotspot-subpage>', () => {
  let hotspotSubpage: SettingsHotspotSubpageElement;
  let hotspotConfig: FakeHotspotConfig;
  let hotspotConfigObserver: CrosHotspotConfigObserverInterface;

  suiteSetup(() => {
    hotspotConfig = new FakeHotspotConfig();
    setHotspotConfigForTesting(hotspotConfig);
  });

  function init(urlParams?: URLSearchParams) {
    hotspotSubpage = document.createElement('settings-hotspot-subpage');
    document.body.appendChild(hotspotSubpage);
    flush();

    hotspotConfigObserver = {
      onHotspotInfoChanged() {
        hotspotConfig.getHotspotInfo().then(response => {
          hotspotSubpage.hotspotInfo = response.hotspotInfo;
        });
      },
    };
    hotspotConfig.addObserver(
        hotspotConfigObserver as CrosHotspotConfigObserverRemote);
    const hotspotInfo = {
      state: HotspotState.kDisabled,
      allowStatus: HotspotAllowStatus.kAllowed,
      clientCount: 0,
      config: {
        autoDisable: true,
        ssid: 'test_ssid',
        passphrase: 'test_passphrase',
      },
    } as HotspotInfo;
    hotspotConfig.setFakeHotspotInfo(hotspotInfo);
    Router.getInstance().navigateTo(routes.HOTSPOT_DETAIL, urlParams);
    return flushTasks();
  }

  function queryEnableHotspotToggle(): CrToggleElement|null {
    return hotspotSubpage.shadowRoot!.querySelector<CrToggleElement>(
        '#enableHotspotToggle');
  }

  function queryHotspotAutoDisableToggle(): SettingsToggleButtonElement|null {
    return hotspotSubpage.shadowRoot!
        .querySelector<SettingsToggleButtonElement>(
            '#hotspotAutoDisableToggle');
  }

  function queryConfigureButton(): CrButtonElement|null {
    return hotspotSubpage.shadowRoot!.querySelector<CrButtonElement>(
        '#configureButton');
  }


  teardown(() => {
    hotspotConfig.reset();
    hotspotSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Toggle button state and a11y', async () => {
    await init();
    const enableHotspotToggle = queryEnableHotspotToggle();
    assertTrue(!!enableHotspotToggle);
    assertFalse(enableHotspotToggle.checked);

    // Simulate clicking toggle to turn on hotspot and fail.
    hotspotConfig.setFakeEnableHotspotResult(
        HotspotControlResult.kNetworkSetupFailure);
    enableHotspotToggle.click();
    await flushTasks();
    // Toggle should be off.
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);

    // Simulate clicking toggle to turn on hotspot and succeed.
    let a11yMessagesEventPromise =
        eventToPromise('cr-a11y-announcer-messages-sent', document.body);
    hotspotConfig.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    enableHotspotToggle.click();
    await flushTasks();
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
    await flushTasks();
    // Toggle should be off
    assertFalse(enableHotspotToggle.checked);
    assertFalse(enableHotspotToggle.disabled);
    a11yMessagesEvent = await a11yMessagesEventPromise;
    assertTrue(a11yMessagesEvent.detail.messages.includes(
        hotspotSubpage.i18n('hotspotDisabledA11yLabel')));

    // Simulate state becoming kEnabling.
    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();
    // Toggle should not be disabled.
    assertFalse(enableHotspotToggle.disabled);
    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);

    // Simulate AllowStatus becoming kDisallowedByPolicy.
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushTasks();
    // Toggle should be disabled.
    assertTrue(enableHotspotToggle.disabled);
  });

  test('UI state test', async () => {
    await init();
    // Simulate hotspot state is disabled.
    const hotspotOnOffLabel =
        hotspotSubpage.shadowRoot!.querySelector('#hotspotToggleText');
    const enableToggle = queryEnableHotspotToggle();
    const hotspotNameElement =
        hotspotSubpage.shadowRoot!.querySelector('#hotspotSSID');
    const connectedClientCountRow =
        hotspotSubpage.shadowRoot!.querySelector<HTMLElement>(
            '#connectedDeviceCountRow');
    const connectedClientCount =
        hotspotSubpage.shadowRoot!.querySelector('#connectedDeviceCount');

    assertTrue(!!hotspotOnOffLabel);
    assertTrue(!!enableToggle);
    assertTrue(!!hotspotNameElement);
    assertTrue(!!connectedClientCountRow);
    assertTrue(!!connectedClientCount);

    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent!.trim());
    assertEquals('test_ssid', hotspotNameElement.textContent!.trim());
    assertFalse(
        enableToggle.checked, 'Enable hotspot toggle should not be checked');
    assertTrue(
        connectedClientCountRow.hidden,
        'Connected device count row should be hidden');

    // Simulate turning on hotspot.
    hotspotConfig.setFakeEnableHotspotResult(HotspotControlResult.kSuccess);
    hotspotConfig.enableHotspot();
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOn'),
        hotspotOnOffLabel.textContent!.trim());
    assertTrue(enableToggle.checked, 'Enable hotspot toggle should be checked');
    assertFalse(
        connectedClientCountRow.hidden,
        'Connected device count row should not be hidden');

    // Simulate turning off hotspot.
    hotspotConfig.setFakeDisableHotspotResult(HotspotControlResult.kSuccess);
    hotspotConfig.disableHotspot();
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent!.trim());
    assertFalse(
        enableToggle.checked, 'Enable hotspot toggle should not be checked');
    assertTrue(
        connectedClientCountRow.hidden,
        'Connected device count row should be hidden');

    // Verify toggle is able to turn on/off by CrosHotspotConfig even when it is
    // disabled by policy.
    hotspotConfig.setFakeHotspotAllowStatus(
        HotspotAllowStatus.kDisallowedByPolicy);
    await flushTasks();
    // Toggle should be disabled.
    assertTrue(
        enableToggle.disabled, 'Enable hotspot toggle should be disabled');

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabling);
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateTurningOn'),
        hotspotOnOffLabel.textContent!.trim());
    assertTrue(
        enableToggle.checked,
        'Enable hotspot toggle should not checked when hotspot enabling');
    assertTrue(
        connectedClientCountRow.hidden,
        'Connected device count row should be hidden when hotspot enabling');

    hotspotConfig.setFakeHotspotState(HotspotState.kEnabled);
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOn'),
        hotspotOnOffLabel.textContent!.trim());
    assertTrue(
        enableToggle.checked,
        'Enable hotspot toggle should be checked when hotspot enabled');
    assertFalse(
        connectedClientCountRow.hidden,
        'Connected device count row should be not hidden when hotspot enabled');
    assertEquals('0', connectedClientCount.textContent!.trim());

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabling);
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateTurningOff'),
        hotspotOnOffLabel.textContent!.trim());
    assertFalse(
        enableToggle.checked,
        'Enable hotspot toggle should not be checked when hotspot disabling');
    assertFalse(
        connectedClientCountRow.hidden,
        'Connected device count row should be shown when hotspot disabling');
    assertEquals('0', connectedClientCount.textContent!.trim());

    hotspotConfig.setFakeHotspotState(HotspotState.kDisabled);
    await flushTasks();
    assertEquals(
        hotspotSubpage.i18n('hotspotSummaryStateOff'),
        hotspotOnOffLabel.textContent!.trim());
    assertFalse(
        enableToggle.checked,
        'Enable hotspot toggle should not be checked when hotspot disabled');
    assertTrue(
        connectedClientCountRow.hidden,
        'Connected device count row should be hidden when hotspot disabled');

    hotspotConfig.setFakeHotspotActiveClientCount(6);
    await flushTasks();
    assertEquals('6', connectedClientCount.textContent!.trim());

    const config = {ssid: 'new_ssid'} as HotspotConfig;

    hotspotConfig.setFakeHotspotConfig(config);
    await flushTasks();
    assertEquals('new_ssid', hotspotNameElement.textContent!.trim());

    // Verifies UI with undefined hotspot config
    hotspotConfig.setFakeHotspotConfig(undefined);
    await flushTasks();
    assertEquals('', hotspotNameElement.textContent?.trim());
  });

  test('Auto disable toggle', async () => {
    await init();
    let autoDisableToggle = queryHotspotAutoDisableToggle();
    assertTrue(!!autoDisableToggle);
    assertTrue(autoDisableToggle.checked);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kSuccess);
    autoDisableToggle.click();
    await flushTasks();
    assertFalse(autoDisableToggle.checked);

    hotspotConfig.setFakeSetHotspotConfigResult(
        SetHotspotConfigResult.kFailedInvalidConfiguration);
    autoDisableToggle.click();
    await flushTasks();
    assertFalse(autoDisableToggle.checked);

    // Verifies that the toggle should be hidden if the hotspot config is
    // undefined.
    hotspotConfig.setFakeHotspotConfig(undefined);
    await flushTasks();
    autoDisableToggle = queryHotspotAutoDisableToggle();
    assertEquals(null, autoDisableToggle);
  });

  test('Hide configure button when hotspot config is undefined', async () => {
    await init();
    const configureButton = queryConfigureButton();
    assertTrue(!!configureButton, 'Hotspot configure button does not exist');
    assertFalse(configureButton.hidden);

    hotspotConfig.setFakeHotspotConfig(undefined);
    await flushTasks();
    assertTrue(configureButton.hidden);
  });

  test(
      'Click on configure button should fire show-hotspot-config-dialog event',
      async () => {
        await init();
        const configureButton = queryConfigureButton();
        assertTrue(
            !!configureButton, 'Hotspot configure button does not exist');
        assertFalse(configureButton.hidden);

        const showHotspotConfigDialogEvent =
            eventToPromise('show-hotspot-config-dialog', hotspotSubpage);
        configureButton.click();
        await Promise.all([showHotspotConfigDialogEvent, flushTasks()]);
      });

  test('Deep link to hotspot on/off toggle', async () => {
    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kHotspotOnOff.toString());
    await init(params);

    const deepLinkElement = queryEnableHotspotToggle();
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(hotspotSubpage);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'On startup enable/disable Hotspot toggle should be focused for ' +
            'settingId=30.');
  });

  test('Deep link to auto disable hotspot toggle', async () => {
    const params = new URLSearchParams();
    params.append(
        'settingId', settingMojom.Setting.kHotspotAutoDisabled.toString());
    await init(params);

    const autoDisableToggle = queryHotspotAutoDisableToggle();
    assertTrue(!!autoDisableToggle);
    const deepLinkElement =
        autoDisableToggle.shadowRoot!.querySelector('cr-toggle');
    await waitAfterNextRender(hotspotSubpage);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'On startup auto turn off hotspot toggle should be focused for ' +
            'settingId=31.');
  });
});
