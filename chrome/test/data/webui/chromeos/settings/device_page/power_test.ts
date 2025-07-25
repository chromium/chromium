// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsPowerElement} from 'chrome://os-settings/lazy_load.js';
import type {BatteryStatus, CrButtonElement, CrDialogElement, CrPolicyIndicatorElement, CrRadioButtonElement, PowerSource, SettingsToggleButtonElement, SettingsToggleV2Element} from 'chrome://os-settings/os_settings.js';
import {DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, OptimizedChargingStrategy, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
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
  let powerSubpage: SettingsPowerElement;
  let browserProxy: TestDevicePageBrowserProxy;

  async function initSubpage(): Promise<void> {
    clearBody();
    powerSubpage = document.createElement('settings-power');
    powerSubpage.prefs = getFakePrefs();
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
    chargeLimit?: boolean;
    optimizedChargingStrategy?: OptimizedChargingStrategy;
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
    chargeLimit = false,
    optimizedChargingStrategy =
        OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING,
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
      chargeLimit,
      optimizedChargingStrategy,
      batterySaverFeatureEnabled,
    });
    flush();
  }

  setup(async () => {
    // Adaptive charging setting should be shown.
    loadTimeData.overrideValues({
      isAdaptiveChargingSupported: true,
      isBatteryChargeLimitAvailable: false,
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

    test('Power idle heading is not visible', () => {
      const powerIdleLabel =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#powerIdleLabel');
      assertFalse(isVisible(powerIdleLabel));
    });
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

  suite('Optimized Charging', () => {
    function queryOptimizedChargingRow(): HTMLElement|null {
      return powerSubpage.shadowRoot!.querySelector<HTMLElement>(
          '#optimizedChargingSettingsRow');
    }

    function queryOptimizedChargingChangeButton(): CrButtonElement|null {
      return powerSubpage.shadowRoot!.querySelector<CrButtonElement>(
          '#optimizedChargingChangeButton');
    }

    function queryOptimizedChargingToggle(): SettingsToggleV2Element|null {
      return powerSubpage.shadowRoot!.querySelector<SettingsToggleV2Element>(
          '#optimizedChargingToggle');
    }

    function queryOptimizedChargingSublabelSpan(): HTMLElement|null {
      return powerSubpage.shadowRoot!.querySelector<HTMLElement>(
          '#optimizedChargingSublabel .sub-label-text');
    }

    function queryOptimizedChargingDialog(): CrDialogElement|null {
      const dialog =
          powerSubpage.shadowRoot!
              .querySelector<HTMLElement>('#optimizedChargingDialog')
              ?.shadowRoot!.querySelector<CrDialogElement>('#dialog') ??
          null;
      return dialog;
    }

    function queryAdaptiveChargingRadioButtonFromDialog(): CrRadioButtonElement|
        null {
      return queryOptimizedChargingDialog()
                 ?.querySelector<CrRadioButtonElement>('#adaptiveCharging') ??
          null;
    }

    function queryChargeLimitRadioButtonFromDialog(): CrRadioButtonElement|
        null {
      return queryOptimizedChargingDialog()
                 ?.querySelector<CrRadioButtonElement>('#chargeLimit') ??
          null;
    }

    function queryCancelButtonFromDialog(): CrButtonElement|null {
      return queryOptimizedChargingDialog()?.querySelector<CrButtonElement>(
                 '#cancel') ??
          null;
    }

    function queryDoneButtonFromDialog(): CrButtonElement|null {
      return queryOptimizedChargingDialog()?.querySelector<CrButtonElement>(
                 '#done') ??
          null;
    }

    function queryOptimizedChargingPolicyIndicator(): CrPolicyIndicatorElement|
        null {
      return powerSubpage.shadowRoot!.querySelector<CrPolicyIndicatorElement>(
          '#optimizedChargingManagedIndicator');
    }

    async function openChangeDialog() {
      const changeButton = queryOptimizedChargingChangeButton();
      assertTrue(!!changeButton, 'Change button should exist');

      // Click the button to show the dialog.
      changeButton.click();

      // Ensure the dialog element is stamped into the DOM.
      await flushTasks();
    }

    function assertDialogVisible(dialog: CrDialogElement|null): void {
      assertTrue(!!dialog);

      // CrDialogs themselves don't have width and height in tests for
      // optimization reasons, so instead assert that the contents (title, body,
      // and button container) are visible.
      const title = dialog.querySelector('[slot="title"]');
      const body = dialog.querySelector('[slot="body"]');
      const buttonContainer = dialog.querySelector('[slot="button-container"]');

      assertTrue(isVisible(title), 'Dialog have a visible title.');
      assertTrue(isVisible(body), 'Dialog have a visible body.');
      assertTrue(
          isVisible(buttonContainer),
          'Dialog have a visible button container.');
    }

    function setStrategyPref(strategy: OptimizedChargingStrategy): void {
      const newPrefs = getFakePrefs();
      newPrefs.power.optimized_charging_strategy.value = strategy;
      powerSubpage.prefs = newPrefs;
      flush();
    }

    async function initTestState(
        batteryStatus: BatteryStatus|undefined,
        featureEnabled: boolean): Promise<void> {
      loadTimeData.overrideValues({
        isBatteryChargeLimitAvailable: featureEnabled,
      });
      // Re-initialize the subpage with the new loadTimeData.
      await initSubpage();

      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    }

    setup(async () => {
      // Simulate battery presence for relevant tests.
      const batteryStatus: BatteryStatus = {
        present: true,
        charging: false,
        calculating: false,
        percent: 50,
        statusText: '5 hours left',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    test(
        'has correct sublabel for adaptive charging strategy', async () => {
          // Setup test with adaptive charging being supported.
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Case 1: Adaptive Charging is the selected strategy.
          sendPowerManagementSettings({
            optimizedChargingStrategy:
                OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING,
          });

          const sublabelSpan = queryOptimizedChargingSublabelSpan();
          assertTrue(!!sublabelSpan);

          assertEquals(
              powerSubpage.i18n('powerAdaptiveChargingLabel'),
              sublabelSpan.innerText.trim());
        });

    test(
        'has correct sublabel for charge limit strategy', async () => {
          // Setup test with adaptive charging being supported.
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Case 2: Charge Limit is the selected strategy.
          sendPowerManagementSettings({
            optimizedChargingStrategy:
                OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT,
          });

          const sublabelSpan = queryOptimizedChargingSublabelSpan();
          assertTrue(!!sublabelSpan);


          assertEquals(
              powerSubpage.i18n('powerBatteryChargeLimitLabel'),
              sublabelSpan.innerText.trim());
        });

    test(
        'is visible with undefined battery status, and feature enabled.',
        async () => {
          // Case 1: batteryStatus is undefined
          await initTestState(undefined, /*featureEnabled=*/ true);
          assertTrue(isVisible(queryOptimizedChargingRow()));
        });

    test(
        'is hidden with undefined battery status, and feature disabled.',
        async () => {
          await initTestState(undefined, /*featureEnabled=*/ false);
          assertFalse(isVisible(queryOptimizedChargingRow()));
        });

    test(
        'is visible with a battery present, and feature enabled.', async () => {
          // Case 2: batteryStatus.present = true
          const mockBatteryStatus: BatteryStatus = {
            present: true,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: 'stub',
          };

          await initTestState(mockBatteryStatus, /*featureEnabled=*/ true);
          assertTrue(isVisible(queryOptimizedChargingRow()));
        });

    test(
        'is hidden with a battery present, and feature disabled.', async () => {
          // Case 2: batteryStatus.present = true
          const mockBatteryStatus: BatteryStatus = {
            present: true,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: 'stub',
          };

          await initTestState(mockBatteryStatus, /*featureEnabled=*/ false);
          assertFalse(isVisible(queryOptimizedChargingRow()));
        });

    test(
        'is visible without a battery present, and feature enabled.',
        async () => {
          // Case 3: batteryStatus.present = false
          // (This can happen when there is no battery, and a low power adapter
          // is plugged in, and it is discharging).
          const mockBatteryStatus: BatteryStatus = {
            present: false,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: 'stub',
          };

          await initTestState(mockBatteryStatus, /*featureEnabled=*/ true);
          assertTrue(isVisible(queryOptimizedChargingRow()));
        });

    test(
        'is hidden without a battery present, and feature disabled.',
        async () => {
          // Case 3: batteryStatus.present = false
          const mockBatteryStatus = {
            present: false,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: 'stub',
          };

          await initTestState(mockBatteryStatus, /*featureEnabled=*/ false);
          assertFalse(isVisible(queryOptimizedChargingRow()));
        });

    test('is deep-linkable.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      await deepLinkToSetting(settingMojom.Setting.kOptimizedCharging);
      const optimizedChargingToggle = queryOptimizedChargingToggle();
      assertTrue(!!optimizedChargingToggle);
      await assertElementIsDeepLinked(optimizedChargingToggle);
    });

    test('Charge limit is deep-linkable.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      await deepLinkToSetting(settingMojom.Setting.kChargeLimit);
      const optimizedChargingChangeButton =
          queryOptimizedChargingChangeButton();
      assertTrue(!!optimizedChargingChangeButton);
      await assertElementIsDeepLinked(optimizedChargingChangeButton);
    });

    test(
        'Adaptive charging is deep-linkable in optimized charging row.',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          await deepLinkToSetting(settingMojom.Setting.kAdaptiveCharging);
          const optimizedChargingChangeButton =
              queryOptimizedChargingChangeButton();
          assertTrue(!!optimizedChargingChangeButton);
          await assertElementIsDeepLinked(optimizedChargingChangeButton);
        });

    test('can open the change dialog.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      // Verify the dialog is not in the DOM initially.
      assertFalse(
          !!queryOptimizedChargingDialog(),
          'Dialog should not exist before the button is clicked');

      await openChangeDialog();

      // Now that the dialog's state is officially 'open', it should be visible.
      assertDialogVisible(queryOptimizedChargingDialog());
    });

    test(
        'dialog has the adaptive charging option selected based on pref.',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Explicitly set the strategy to adaptive charging.
          setStrategyPref(OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING);

          await openChangeDialog();

          const adaptiveChargingRadio =
              queryAdaptiveChargingRadioButtonFromDialog();
          const chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
          assertTrue(!!adaptiveChargingRadio);
          assertTrue(!!chargeLimitRadio);

          assertTrue(adaptiveChargingRadio.checked);
          assertFalse(chargeLimitRadio.checked);
        });

    test(
        'dialog has the charge limit option selected based on pref.',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Set the strategy to charge limit.
          setStrategyPref(OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT);

          await openChangeDialog();

          const adaptiveChargingRadio =
              queryAdaptiveChargingRadioButtonFromDialog();
          const chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
          assertTrue(!!adaptiveChargingRadio);
          assertTrue(!!chargeLimitRadio);

          assertFalse(adaptiveChargingRadio.checked);
          assertTrue(chargeLimitRadio.checked);
        });

    test('dialog saves changes made to radio group.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      // Start the test with the previously selected strategy as Charge Limit.
      setStrategyPref(OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT);

      // Open the change dialog
      await openChangeDialog();

      // Get the radio buttons.
      let adaptiveChargingRadio = queryAdaptiveChargingRadioButtonFromDialog();
      let chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
      assertTrue(!!adaptiveChargingRadio);
      assertTrue(!!chargeLimitRadio);

      // Select the Adaptive charging radio element.
      assertFalse(adaptiveChargingRadio.checked);
      adaptiveChargingRadio.click();
      assertTrue(adaptiveChargingRadio.checked);

      // Attempt to save the changes.
      let doneButton = queryDoneButtonFromDialog();
      assertTrue(!!doneButton);
      doneButton.click();
      await flushTasks();
      await waitAfterNextRender(powerSubpage);

      // Assert dialog is closed.
      assertFalse(!!queryOptimizedChargingDialog());
      assertEquals(
          powerSubpage.prefs.power.optimized_charging_strategy.value,
          OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING);

      // Reopen dialog.
      await openChangeDialog();

      // Refetch the radio buttons.
      adaptiveChargingRadio = queryAdaptiveChargingRadioButtonFromDialog();
      chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
      assertTrue(!!adaptiveChargingRadio);
      assertTrue(!!chargeLimitRadio);

      // Assert the option that was clicked is saved and selected when the
      // dialog reopens.
      assertTrue(adaptiveChargingRadio.checked);
      assertFalse(chargeLimitRadio.checked);

      // Do it again with the other radio button.
      chargeLimitRadio.click();
      assertFalse(adaptiveChargingRadio.checked);
      assertTrue(chargeLimitRadio.checked);

      // Attempt to save the changes.
      doneButton = queryDoneButtonFromDialog();
      assertTrue(!!doneButton);
      doneButton.click();
      await flushTasks();
      await waitAfterNextRender(powerSubpage);

      // Assert dialog is closed.
      assertFalse(!!queryOptimizedChargingDialog());
      assertEquals(
          powerSubpage.prefs.power.optimized_charging_strategy.value,
          OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT);

      // Reopen dialog.
      await openChangeDialog();

      // Refetch the radio buttons.
      adaptiveChargingRadio = queryAdaptiveChargingRadioButtonFromDialog();
      chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
      assertTrue(!!adaptiveChargingRadio);
      assertTrue(!!chargeLimitRadio);

      // Assert the option that was clicked is saved and selected when the
      // dialog reopens.
      assertFalse(adaptiveChargingRadio.checked);
      assertTrue(chargeLimitRadio.checked);
    });

    test('dialog does not save changes when cancelled.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();
      // Set up the test with a default selected Charge Limit strategy, and with
      // the being opened.
      setStrategyPref(OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT);
      await openChangeDialog();
      let adaptiveChargingRadio = queryAdaptiveChargingRadioButtonFromDialog();
      let chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
      assertTrue(!!adaptiveChargingRadio);
      assertTrue(!!chargeLimitRadio);

      // Verify that the Charge Limit radio button is selected.
      assertTrue(chargeLimitRadio.checked);
      assertFalse(adaptiveChargingRadio.checked);

      // Select the Adaptive charging radio element.
      adaptiveChargingRadio.click();
      assertFalse(chargeLimitRadio.checked);
      assertTrue(adaptiveChargingRadio.checked);

      // Cancel the dialog.
      const cancelButton = queryCancelButtonFromDialog();
      assertTrue(!!cancelButton);
      cancelButton.click();
      await flushTasks();
      await waitAfterNextRender(powerSubpage);

      // Assert the dialog is gone.
      assertFalse(!!queryOptimizedChargingDialog());
      assertEquals(
          powerSubpage.prefs.power.optimized_charging_strategy.value,
          OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT);

      // Reopen the dialog.
      await openChangeDialog();

      // Refetch the radio elements.
      adaptiveChargingRadio = queryAdaptiveChargingRadioButtonFromDialog();
      chargeLimitRadio = queryChargeLimitRadioButtonFromDialog();
      assertTrue(!!adaptiveChargingRadio);
      assertTrue(!!chargeLimitRadio);

      // Verify the Charge Limit radio button is still selected by default.
      assertTrue(chargeLimitRadio.checked);
      assertFalse(adaptiveChargingRadio.checked);
    });

    test(
        'should disable change button and slider toggle when managed.',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Set the adaptive charging pref to unmanaged (user-controlled).
          sendPowerManagementSettings({adaptiveChargingManaged: false});
          await flushTasks();
          await waitAfterNextRender(powerSubpage);

          // Assert the Change Button and Slider toggle are enabled.
          assertFalse(queryOptimizedChargingChangeButton()?.disabled ?? true);
          assertFalse(queryOptimizedChargingToggle()?.disabled ?? true);

          // Set the adaptive charging pref to managed (policy-controlled).
          sendPowerManagementSettings({adaptiveChargingManaged: true});
          await flushTasks();
          await waitAfterNextRender(powerSubpage);

          // Assert the Change Button and Slider toggle are newly disabled.
          assertTrue(queryOptimizedChargingChangeButton()?.disabled ?? false);
          assertTrue(queryOptimizedChargingToggle()?.disabled ?? false);
        });

    test('should have a policy indicator present when managed.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      // Set the adaptive charging pref to unmanaged (user-controlled).
      sendPowerManagementSettings({adaptiveChargingManaged: false});
      await flushTasks();
      await waitAfterNextRender(powerSubpage);

      // Assert the policy indicator is invisible.
      assertFalse(isVisible(queryOptimizedChargingPolicyIndicator()));

      // Set the adaptive charging pref to managed (policy-controlled).
      sendPowerManagementSettings({adaptiveChargingManaged: true});
      await flushTasks();
      await waitAfterNextRender(powerSubpage);

      // Assert the policy indicator is newly visible.
      assertTrue(isVisible(queryOptimizedChargingPolicyIndicator()));
    });

    test('toggle state reflects the pref values.', async () => {
      loadTimeData.overrideValues({
        isAdaptiveChargingEnabled: true,
        isBatteryChargeLimitAvailable: true,
      });
      await initSubpage();

      // Get the unchecked toggle.
      const toggle = queryOptimizedChargingToggle();
      assertTrue(!!toggle);
      assertFalse(toggle.checked);

      // Enable Adaptive Charging.
      sendPowerManagementSettings({
        adaptiveCharging: true,
        chargeLimit: false,
      });

      assertTrue(toggle.checked);

      // Disable everything
      sendPowerManagementSettings({
        adaptiveCharging: false,
        chargeLimit: false,
      });

      assertFalse(toggle.checked);

      // Enable Charge Limit
      sendPowerManagementSettings({
        adaptiveCharging: false,
        chargeLimit: true,
      });

      assertTrue(toggle.checked);
    });

    test(
        'clicking toggle calls setOptimizedCharging with correct strategy',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Send an initial state with a specific strategy (e.g., Charge Limit)
          // and ensure the feature is disabled so the toggle is off.
          const initialStrategy =
              OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT;
          sendPowerManagementSettings({
            optimizedChargingStrategy: initialStrategy,
            adaptiveCharging: false,
            chargeLimit: false,
          });
          await flushTasks();

          const toggle = queryOptimizedChargingToggle();
          assertTrue(!!toggle, 'Optimized charging toggle should be present');
          assertFalse(toggle.checked, 'Toggle should be off initially');

          // Reset the proxy method tracker before the action.
          browserProxy.resetResolver('setOptimizedCharging');

          // Click the toggle to turn it on.
          toggle.$.control.click();

          // Verify that the browser proxy was called with the correct
          // strategy and the new enabled state.
          const [strategy, enabled] =
              await browserProxy.whenCalled('setOptimizedCharging');
          assertEquals(
              initialStrategy, strategy,
              'Strategy passed to proxy is incorrect');
          assertTrue(enabled, 'Enabled state passed to proxy should be true');
        });

    test(
        'clicking toggle off calls setOptimizedCharging correctly',
        async () => {
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Send an initial state where the feature is ON, which will
          // turn the toggle on.
          const initialStrategy =
              OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING;
          sendPowerManagementSettings({
            optimizedChargingStrategy: initialStrategy,
            adaptiveCharging: true,
            chargeLimit: false,
          });
          await flushTasks();

          const toggle = queryOptimizedChargingToggle();
          assertTrue(!!toggle, 'Optimized charging toggle should be present');
          assertTrue(toggle.checked, 'Toggle should be on initially');

          // Reset the proxy method tracker before the action.
          browserProxy.resetResolver('setOptimizedCharging');

          // Click the toggle to turn it off.
          toggle.$.control.click();

          // Verify that the browser proxy was called to disable the
          // feature. The `enabled` flag should now be false.
          const [strategy, enabled] =
              await browserProxy.whenCalled('setOptimizedCharging');
          assertEquals(
              initialStrategy, strategy,
              'Strategy passed to proxy is incorrect');
          assertFalse(enabled, 'Enabled state passed to proxy should be false');
        });

    test(
        'receiving a new strategy while enabled hotswaps the backend pref',
        async () => {
          // Ensure the feature is available.
          loadTimeData.overrideValues({
            isAdaptiveChargingEnabled: true,
            isBatteryChargeLimitAvailable: true,
          });
          await initSubpage();

          // Send an initial state where the feature is ON with one
          // strategy (e.g., Adaptive Charging).
          sendPowerManagementSettings({
            optimizedChargingStrategy:
                OptimizedChargingStrategy.STRATEGY_ADAPTIVE_CHARGING,
            adaptiveCharging: true,
            chargeLimit: false,
          });
          await flushTasks();

          const toggle = queryOptimizedChargingToggle();
          assertTrue(!!toggle, 'Optimized charging toggle should be present');
          assertTrue(toggle.checked, 'Toggle should be on initially');

          // Reset the proxy method tracker before the action.
          browserProxy.resetResolver('setOptimizedCharging');

          const newStrategy = OptimizedChargingStrategy.STRATEGY_CHARGE_LIMIT;

          // Send a power management update with a new strategy,
          // while the feature remains enabled. This simulates an external
          // change, for example, from a policy update.
          sendPowerManagementSettings({
            optimizedChargingStrategy: newStrategy,
            adaptiveCharging: true,  // The overall feature is still "on".
            chargeLimit: false,
          });

          // The "hotswap" logic in the frontend should have triggered a call
          // to the browser proxy to activate the new strategy on the backend.
          const [strategy, enabled] =
              await browserProxy.whenCalled('setOptimizedCharging');
          assertEquals(
              newStrategy, strategy,
              'Proxy should be called with the new strategy');
          assertTrue(
              enabled,
              'Proxy should be called with enabled=true to activate the new strategy');
        });
  });
});
