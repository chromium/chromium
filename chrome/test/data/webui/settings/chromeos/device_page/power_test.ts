// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPowerElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, PowerSource, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-power>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let powerSubpage: SettingsPowerElement;
  let browserProxy: TestDevicePageBrowserProxy;

  async function initSubpage(): Promise<void> {
    clearBody();
    powerSubpage = document.createElement('settings-power');
    document.body.appendChild(powerSubpage);
    await flushTasks();
  }

  async function deepLinkToSetting(setting: settingMojom.Setting):
      Promise<void> {
    const settingId = setting.toString();
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.POWER, params);
    await flushTasks();
  }

  async function assertElementIsDeepLinked(element: HTMLElement):
      Promise<void> {
    assertTrue(isVisible(element));
    await waitAfterNextRender(element);
    assertEquals(element, powerSubpage.shadowRoot!.activeElement);
  }

  function setPowerSources(
      sources: PowerSource[], powerSourceId: string,
      isExternalPowerUSB: boolean, isExternalPowerAC: boolean): void {
    const sourcesCopy = sources.map((source) => Object.assign({}, source));
    webUIListenerCallback(
        'power-sources-changed', sourcesCopy, powerSourceId, isExternalPowerUSB,
        isExternalPowerAC);
    flush();
  }

  interface PowerManagementSettingsParams {
    possibleAcIdleBehaviors?: IdleBehavior[];
    possibleBatteryIdleBehaviors?: IdleBehavior[];
    currentAcIdleBehavior?: IdleBehavior;
    currentBatteryIdleBehavior?: IdleBehavior;
    acIdleManaged?: boolean;
    batteryIdleManaged?: boolean;
    lidClosedBehavior?: LidClosedBehavior;
    lidClosedControlled?: boolean;
    hasLid?: boolean;
    adaptiveCharging?: boolean;
    adaptiveChargingManaged?: boolean;
    batterySaverFeatureEnabled?: boolean;
  }

  function sendPowerManagementSettings({
    possibleAcIdleBehaviors =
        [
          IdleBehavior.DISPLAY_OFF_SLEEP,
          IdleBehavior.DISPLAY_OFF,
          IdleBehavior.DISPLAY_ON,
          IdleBehavior.SHUT_DOWN,
          IdleBehavior.STOP_SESSION,
        ],
    possibleBatteryIdleBehaviors =
        [
          IdleBehavior.DISPLAY_OFF_SLEEP,
          IdleBehavior.DISPLAY_OFF,
          IdleBehavior.DISPLAY_ON,
          IdleBehavior.SHUT_DOWN,
          IdleBehavior.STOP_SESSION,
        ],
    currentAcIdleBehavior = IdleBehavior.DISPLAY_OFF_SLEEP,
    currentBatteryIdleBehavior = IdleBehavior.DISPLAY_OFF_SLEEP,
    acIdleManaged = false,
    batteryIdleManaged = false,
    lidClosedBehavior = LidClosedBehavior.SUSPEND,
    lidClosedControlled = false,
    hasLid = true,
    adaptiveCharging = false,
    adaptiveChargingManaged = false,
    batterySaverFeatureEnabled = true,
  }: PowerManagementSettingsParams = {}): void {
    webUIListenerCallback('power-management-settings-changed', {
      possibleAcIdleBehaviors,
      possibleBatteryIdleBehaviors,
      currentAcIdleBehavior,
      currentBatteryIdleBehavior,
      acIdleManaged,
      batteryIdleManaged,
      lidClosedBehavior,
      lidClosedControlled,
      hasLid,
      adaptiveCharging,
      adaptiveChargingManaged,
      batterySaverFeatureEnabled,
    });
    flush();
  }

  setup(async () => {
    // Adaptive charging setting should be shown.
    loadTimeData.overrideValues({
      isAdaptiveChargingEnabled: true,
    });

    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    Router.getInstance().navigateTo(routes.POWER);
    await initSubpage();

    assertEquals(1, browserProxy.getCallCount('updatePowerStatus'));
    assertEquals(
        1, browserProxy.getCallCount('requestPowerManagementSettings'));

    // Use default power management settings.
    sendPowerManagementSettings();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  suite('When battery status is present', () => {
    setup(async () => {
      const batteryStatus = {
        present: true,
        charging: false,
        calculating: false,
        percent: 50,
        statusText: '5 hours left',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    suite('AC idle', () => {
      function queryAcIdleSelect(): HTMLSelectElement|null {
        return powerSubpage.shadowRoot!.querySelector<HTMLSelectElement>(
            '#acIdleSelect');
      }

      function queryAcIdleManagedIndicator(): HTMLElement|null {
        return powerSubpage.shadowRoot!.querySelector<HTMLElement>(
            '#acIdleManagedIndicator');
      }

      test('Select element is deep-linkable', async () => {
        await deepLinkToSetting(
            settingMojom.Setting.kPowerIdleBehaviorWhileCharging);

        const acIdleSelect = queryAcIdleSelect();
        assertTrue(!!acIdleSelect);
        await assertElementIsDeepLinked(acIdleSelect);
      });

      test('AC idle select can be set by user', () => {
        sendPowerManagementSettings({
          acIdleManaged: false,
        });

        const acIdleSelect = queryAcIdleSelect();
        assertTrue(!!acIdleSelect);
        assertTrue(isVisible(acIdleSelect));
        assertFalse(acIdleSelect.disabled);
        assertFalse(isVisible(queryAcIdleManagedIndicator()));

        acIdleSelect.value = IdleBehavior.DISPLAY_ON.toString();
        acIdleSelect.dispatchEvent(new CustomEvent('change'));
        assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.acIdleBehavior);
      });

      test('Select element reflects managed setting', () => {
        const acIdleSelect = queryAcIdleSelect();
        assertTrue(!!acIdleSelect);

        sendPowerManagementSettings({
          currentAcIdleBehavior: IdleBehavior.DISPLAY_OFF_SLEEP,
          acIdleManaged: true,
        });
        assertEquals(
            IdleBehavior.DISPLAY_OFF_SLEEP.toString(), acIdleSelect.value);
        assertTrue(isVisible(queryAcIdleManagedIndicator()));

        sendPowerManagementSettings({
          currentAcIdleBehavior: IdleBehavior.DISPLAY_OFF,
          acIdleManaged: true,
        });
        assertEquals(IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
        assertTrue(isVisible(queryAcIdleManagedIndicator()));
      });
    });

    suite('Battery idle', () => {
      function queryBatteryIdleSelect(): HTMLSelectElement|null {
        return powerSubpage.shadowRoot!.querySelector<HTMLSelectElement>(
            '#batteryIdleSelect');
      }

      function queryBatteryIdleManagedIndicator(): HTMLElement|null {
        return powerSubpage.shadowRoot!.querySelector<HTMLElement>(
            '#batteryIdleManagedIndicator');
      }

      test('Select element is deep-linkable', async () => {
        await deepLinkToSetting(
            settingMojom.Setting.kPowerIdleBehaviorWhileOnBattery);

        const batteryIdleSelect = queryBatteryIdleSelect();
        assertTrue(!!batteryIdleSelect);
        await assertElementIsDeepLinked(batteryIdleSelect);
      });

      test('Battery idle select can be set by user', () => {
        sendPowerManagementSettings({
          batteryIdleManaged: false,
        });

        const batteryIdleSelect = queryBatteryIdleSelect();
        assertTrue(!!batteryIdleSelect);
        assertTrue(isVisible(batteryIdleSelect));
        assertFalse(batteryIdleSelect.disabled);
        assertFalse(isVisible(queryBatteryIdleManagedIndicator()));

        batteryIdleSelect.value = IdleBehavior.DISPLAY_ON.toString();
        batteryIdleSelect.dispatchEvent(new CustomEvent('change'));
        assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.batteryIdleBehavior);
      });

      test('Select element reflects managed setting', () => {
        const batteryIdleSelect = queryBatteryIdleSelect();
        assertTrue(!!batteryIdleSelect);

        sendPowerManagementSettings({
          currentBatteryIdleBehavior: IdleBehavior.DISPLAY_OFF_SLEEP,
          batteryIdleManaged: true,
        });

        assertEquals(
            IdleBehavior.DISPLAY_OFF_SLEEP.toString(), batteryIdleSelect.value);
        assertTrue(isVisible(queryBatteryIdleManagedIndicator()));

        sendPowerManagementSettings({
          currentBatteryIdleBehavior: IdleBehavior.DISPLAY_OFF,
          batteryIdleManaged: true,
        });
        assertEquals(
            IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
        assertTrue(isVisible(queryBatteryIdleManagedIndicator()));
      });
    });

    suite('Power sources', () => {
      function queryPowerSourceSelect(): HTMLSelectElement|null {
        return powerSubpage.shadowRoot!.querySelector<HTMLSelectElement>(
            '#powerSource');
      }

      test('When no power source info, select element is hidden', () => {
        setPowerSources(
            /*sources=*/[], /*powerSourceId=*/ '',
            /*isExternalPowerUsb=*/ false, /*isExternalPowerAc=*/ false);

        const powerSourceRow =
            powerSubpage.shadowRoot!.querySelector<HTMLElement>(
                '#powerSourceRow');
        assertTrue(isVisible(powerSourceRow));

        const powerSourceSelect = queryPowerSourceSelect();
        assertFalse(isVisible(powerSourceSelect));
      });

      test('Select element reflects active power source', () => {
        // Attach a dual-role USB device.
        const powerSource: PowerSource = {
          id: '2',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources(
            /*sources=*/[powerSource], /*powerSourceId=*/ '',
            /*isExternalPowerUsb=*/ false, /*isExternalPowerAc=*/ false);

        // "Battery" should be selected.
        const powerSourceSelect = queryPowerSourceSelect();
        assertTrue(!!powerSourceSelect);
        assertTrue(isVisible(powerSourceSelect));
        assertEquals('', powerSourceSelect.value);

        // Select the power source.
        setPowerSources(
            /*sources=*/[powerSource], /*powerSourceId=*/ powerSource.id,
            /*isExternalPowerUsb=*/ true, /*isExternalPowerAc=*/ false);
        assertTrue(isVisible(powerSourceSelect));
        assertEquals(powerSource.id, powerSourceSelect.value);

        // Send another power source; the first should still be selected.
        const otherPowerSource = {...powerSource, id: '3'};
        setPowerSources(
            /*sources=*/[otherPowerSource, powerSource],
            /*powerSourceId=*/ powerSource.id, /*isExternalPowerUsb=*/ true,
            /*isExternalPowerAc=*/ false);
        assertEquals(powerSource.id, powerSourceSelect.value);
      });

      test('Power source can be selected', () => {
        // Attach a dual-role USB device.
        const powerSource: PowerSource = {
          id: '2',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources(
            /*sources=*/[powerSource], /*powerSourceId=*/ '',
            /*isExternalPowerUsb=*/ false, /*isExternalPowerAc=*/ false);

        // Select the device.
        const powerSourceSelect = queryPowerSourceSelect();
        assertTrue(!!powerSourceSelect);
        powerSourceSelect.value =
            (powerSourceSelect.children[1] as HTMLOptionElement).value;
        powerSourceSelect.dispatchEvent(new CustomEvent('change'));
        assertEquals(powerSource.id, browserProxy.powerSourceId);
      });
    });

    suite('Battery saver', () => {
      function queryBatterySaverToggle(): SettingsToggleButtonElement|null {
        return powerSubpage.shadowRoot!
            .querySelector<SettingsToggleButtonElement>('#batterySaverToggle');
      }

      test('Toggle is hidden when feature is disabled', () => {
        sendPowerManagementSettings({batterySaverFeatureEnabled: false});

        const batterySaverToggle = queryBatterySaverToggle();
        assertFalse(isVisible(batterySaverToggle));
      });

      test('Toggle is not disabled when no power sources', () => {
        // There are no power sources.
        setPowerSources(
            /*sources=*/[], /*powerSourceId=*/ '',
            /*isExternalPowerUsb=*/ false, /*isExternalPowerAc=*/ false);
        const batterySaverToggle = queryBatterySaverToggle();
        assertTrue(!!batterySaverToggle);
        assertTrue(isVisible(batterySaverToggle));
        assertFalse(batterySaverToggle.disabled);
      });

      test('Toggle is disabled for AC power source', () => {
        // Connect a dedicated AC power adapter.
        const mainsPowerSource: PowerSource = {
          id: '1',
          is_dedicated_charger: true,
          description: 'USB-C device',
        };
        setPowerSources(
            /*sources=*/[mainsPowerSource],
            /*powerSourceId=*/ mainsPowerSource.id,
            /*isExternalPowerUsb=*/ false, /*isExternalPowerAc=*/ true);

        // Battery saver should be visible but not toggleable.
        const batterySaverToggle = queryBatterySaverToggle();
        assertTrue(!!batterySaverToggle);
        assertTrue(isVisible(batterySaverToggle));
        assertTrue(batterySaverToggle.disabled);
      });

      test('Toggle reflects pref value', () => {
        function setPref(value: boolean): void {
          const newPrefs = getFakePrefs();
          newPrefs.power.cros_battery_saver_active.value = value;
          powerSubpage.prefs = newPrefs;
          flush();
        }

        const batterySaverToggle = queryBatterySaverToggle();
        assertTrue(!!batterySaverToggle);

        setPref(true);
        assertTrue(batterySaverToggle.checked);

        setPref(false);
        assertFalse(batterySaverToggle.checked);
      });
    });

    if (isRevampWayfindingEnabled) {
      test('Power idle heading is not visible', () => {
        const powerIdleLabel =
            powerSubpage.shadowRoot!.querySelector<HTMLElement>(
                '#powerIdleLabel');
        assertFalse(isVisible(powerIdleLabel));
      });
    }
  });

  suite('When battery status is not present', () => {
    setup(async () => {
      const batteryStatus = {
        present: false,
        charging: false,
        calculating: false,
        percent: 100,
        statusText: '',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    test('Power source row is not visible', () => {
      const powerSourceRow =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#powerSourceRow');
      assertFalse(isVisible(powerSourceRow));
    });

    test('Battery saver toggle is not visible', () => {
      const batterySaverToggle =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#batterySaverToggle');
      assertFalse(isVisible(batterySaverToggle));
    });

    test('Deep link to idle behavior while charging setting', async () => {
      await deepLinkToSetting(
          settingMojom.Setting.kPowerIdleBehaviorWhileCharging);

      const deepLinkElement =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#noBatteryAcIdleSelect');
      assertTrue(!!deepLinkElement);
      await assertElementIsDeepLinked(deepLinkElement);
    });

    test('Battery idle row is not visible', () => {
      const batteryIdleSettingBox =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#batteryIdleSettingBox');
      assertFalse(isVisible(batteryIdleSettingBox));
    });

    test('AC idle row is not visible', () => {
      const acIdleSettingBox =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#acIdleSettingBox');
      assertFalse(isVisible(acIdleSettingBox));
    });

    test('No battery AC idle behavior can be set', () => {
      const noBatteryAcIdleSelect =
          powerSubpage.shadowRoot!.querySelector<HTMLSelectElement>(
              '#noBatteryAcIdleSelect');
      assertTrue(!!noBatteryAcIdleSelect);
      assertTrue(isVisible(noBatteryAcIdleSelect));

      // Select a "When idle" selection and expect it to be set.
      noBatteryAcIdleSelect.value = IdleBehavior.DISPLAY_ON.toString();
      noBatteryAcIdleSelect.dispatchEvent(new CustomEvent('change'));
      assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.acIdleBehavior);
    });
  });

  suite('Adaptive charging', () => {
    function queryAdaptiveChargingToggle(): SettingsToggleButtonElement|null {
      return powerSubpage.shadowRoot!
          .querySelector<SettingsToggleButtonElement>(
              '#adaptiveChargingToggle');
    }

    test('Toggle is deep-linkable', async () => {
      await deepLinkToSetting(settingMojom.Setting.kAdaptiveCharging);

      const adaptiveChargingToggle = queryAdaptiveChargingToggle();
      assertTrue(!!adaptiveChargingToggle);
      await assertElementIsDeepLinked(adaptiveChargingToggle);
    });

    test('Toggle reflects managed policy', () => {
      sendPowerManagementSettings({
        adaptiveCharging: true,
        adaptiveChargingManaged: true,
      });

      const adaptiveChargingToggle = queryAdaptiveChargingToggle();
      assertTrue(!!adaptiveChargingToggle);
      assertTrue(adaptiveChargingToggle.checked);
      assertTrue(adaptiveChargingToggle.controlDisabled());
      assertTrue(adaptiveChargingToggle.isPrefEnforced());

      // Must have policy icon.
      assertTrue(isVisible(adaptiveChargingToggle.shadowRoot!.querySelector(
          'cr-policy-pref-indicator')));
    });
  });

  suite('Lid closed', () => {
    function queryLidClosedToggle(): SettingsToggleButtonElement|null {
      return powerSubpage.shadowRoot!
          .querySelector<SettingsToggleButtonElement>('#lidClosedToggle');
    }

    test('Toggle can be updated by user', () => {
      sendPowerManagementSettings({
        hasLid: true,
        lidClosedBehavior: LidClosedBehavior.SUSPEND,
        lidClosedControlled: false,
      });
      const lidClosedToggle = queryLidClosedToggle();
      assertTrue(!!lidClosedToggle);
      assertTrue(isVisible(lidClosedToggle));
      assertTrue(lidClosedToggle.checked);
      assertFalse(lidClosedToggle.isPrefEnforced());

      lidClosedToggle.click();
      assertFalse(lidClosedToggle.checked);
      assertEquals(
          LidClosedBehavior.DO_NOTHING, browserProxy.lidClosedBehavior);

      lidClosedToggle.click();
      assertTrue(lidClosedToggle.checked);
      assertEquals(LidClosedBehavior.SUSPEND, browserProxy.lidClosedBehavior);
    });

    test('Toggle reflects managed behavior', () => {
      const lidClosedToggle = queryLidClosedToggle();
      assertTrue(!!lidClosedToggle);

      // Behavior: Stop session
      sendPowerManagementSettings({
        lidClosedBehavior: LidClosedBehavior.STOP_SESSION,
        lidClosedControlled: true,
        hasLid: true,
      });
      assertEquals(
          powerSubpage.i18n('powerLidSignOutLabel'), lidClosedToggle.label);
      assertTrue(lidClosedToggle.checked);
      assertTrue(lidClosedToggle.isPrefEnforced());

      // Behavior: Shut down
      sendPowerManagementSettings({
        lidClosedBehavior: LidClosedBehavior.SHUT_DOWN,
        lidClosedControlled: true,
        hasLid: true,
      });
      assertEquals(
          powerSubpage.i18n('powerLidShutDownLabel'), lidClosedToggle.label);
      assertTrue(lidClosedToggle.checked);
      assertTrue(lidClosedToggle.isPrefEnforced());

      // Behavior: Do nothing
      sendPowerManagementSettings({
        lidClosedBehavior: LidClosedBehavior.DO_NOTHING,
        lidClosedControlled: true,
        hasLid: true,
      });
      assertEquals(
          powerSubpage.i18n('powerLidSleepLabel'), lidClosedToggle.label);
      assertFalse(lidClosedToggle.checked);
      assertTrue(lidClosedToggle.isPrefEnforced());
    });

    test('Toggle can be deep-linked', async () => {
      sendPowerManagementSettings({hasLid: true});
      await deepLinkToSetting(settingMojom.Setting.kSleepWhenLaptopLidClosed);

      const lidClosedToggle = queryLidClosedToggle();
      assertTrue(!!lidClosedToggle);
      await assertElementIsDeepLinked(lidClosedToggle);
    });

    test('Toggle is hidden when Chromebook has no lid', () => {
      sendPowerManagementSettings({hasLid: false});
      const lidClosedToggle = queryLidClosedToggle();
      assertFalse(isVisible(lidClosedToggle));
    });
  });
});
