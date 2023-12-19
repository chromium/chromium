// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {SettingsPowerElement} from 'chrome://os-settings/lazy_load.js';
import {DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, PowerSource, Route, Router, routes, setDisplayApiForTesting, settingMojom, SettingsDevicePageElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush, microTask} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeSystemDisplay} from '../fake_system_display.js';

import {getFakePrefs} from './device_page_test_util.js';
import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-power> for device page', () => {
  let devicePage: SettingsDevicePageElement;
  let fakeSystemDisplay: FakeSystemDisplay;
  let browserProxy: TestDevicePageBrowserProxy;

  suiteSetup(() => {
    // Disable animations so sub-pages open within one event loop.
    disableAnimationsAndTransitions();
  });

  /**
   * Set enableInputDeviceSettingsSplit feature flag to true for split tests.
   */
  function setDeviceSplitEnabled(isEnabled: boolean): void {
    loadTimeData.overrideValues({
      enableInputDeviceSettingsSplit: isEnabled,
    });
  }

  setup(async () => {
    fakeSystemDisplay = new FakeSystemDisplay();
    setDisplayApiForTesting(fakeSystemDisplay);

    Router.getInstance().navigateTo(routes.BASIC);

    browserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(browserProxy);
    setDeviceSplitEnabled(true);
    // Allow the light DOM to be distributed to os-settings-animated-pages.
    await flushTasks();
  });

  teardown(() => {
    devicePage.remove();
    browserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  async function init(): Promise<void> {
    devicePage = document.createElement('settings-device-page');
    devicePage.prefs = getFakePrefs();
    document.body.appendChild(devicePage);
    flush();
  }

  function showAndGetDeviceSubpage(
      subpage: string, expectedRoute: Route): HTMLElement {
    const row = devicePage.shadowRoot!.querySelector<HTMLButtonElement>(
        `#main #${subpage}Row`);
    assertTrue(!!row);
    row.click();
    assertEquals(expectedRoute, Router.getInstance().currentRoute);
    const page = devicePage.shadowRoot!.querySelector<HTMLElement>(
        'settings-' + subpage);
    assertTrue(!!page);
    return page;
  }

  function sendPowerManagementSettings(
      possibleAcIdleBehaviors: IdleBehavior[],
      possibleBatteryIdleBehaviors: IdleBehavior[],
      currentAcIdleBehavior: IdleBehavior,
      currentBatteryIdleBehavior: IdleBehavior, acIdleManaged: boolean,
      batteryIdleManaged: boolean, lidClosedBehavior: LidClosedBehavior,
      lidClosedControlled: boolean, hasLid: boolean, adaptiveCharging: boolean,
      adaptiveChargingManaged: boolean,
      batterySaverFeatureEnabled: boolean): void {
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

  function selectValue(
      select: HTMLSelectElement, value: IdleBehavior|string): void {
    select.value = value.toString();
    select.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  /**
   * Checks that the deep link to a setting focuses the correct element.
   * @param deepLinkElement The element that should be focused by
   *                                   the deep link
   * @param elementDesc A human-readable description of the element,
   *                              for assertion messages
   */
  async function checkDeepLink(
      route: Route, settingId: string, deepLinkElement: HTMLElement,
      elementDesc: string): Promise<void> {
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(route, params);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        `${elementDesc} should be focused for settingId=${settingId}.`);
  }

  suite('power', () => {
    /**
     * Sets power sources using a deep copy of |sources|.
     */
    function setPowerSources(
        sources: PowerSource[], powerSourceId: string,
        isExternalPowerUSB: boolean, isExternalPowerAC: boolean): void {
      const sourcesCopy = sources.map((source) => Object.assign({}, source));
      webUIListenerCallback(
          'power-sources-changed', sourcesCopy, powerSourceId,
          isExternalPowerUSB, isExternalPowerAC);
    }

    suite('power settings', () => {
      let powerPage: SettingsPowerElement;
      let powerSourceRow: HTMLElement;
      let powerSourceSelect: HTMLSelectElement;
      let acIdleSelect: HTMLSelectElement;
      let lidClosedToggle: SettingsToggleButtonElement;
      let adaptiveChargingToggle: SettingsToggleButtonElement;
      let batterySaverToggle: SettingsToggleButtonElement;

      suiteSetup(() => {
        // Adaptive charging setting should be shown.
        loadTimeData.overrideValues({
          isAdaptiveChargingEnabled: true,
        });
      });

      setup(async () => {
        await init();
        const page = showAndGetDeviceSubpage('power', routes.POWER) as
            SettingsPowerElement;
        powerPage = page;

        const powerSourceRowEl =
            powerPage.shadowRoot!.querySelector<HTMLElement>('#powerSourceRow');
        assertTrue(!!powerSourceRowEl);
        powerSourceRow = powerSourceRowEl;

        const powerSourceSelectEl =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#powerSource');
        assertTrue(!!powerSourceSelectEl);
        powerSourceSelect = powerSourceSelectEl;
        assertEquals(1, browserProxy.getCallCount('updatePowerStatus'));

        const lidClosedToggleEl =
            powerPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#lidClosedToggle');
        assertTrue(!!lidClosedToggleEl);
        lidClosedToggle = lidClosedToggleEl;

        const adaptiveChargingToggleEl =
            powerPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#adaptiveChargingToggle');
        assertTrue(!!adaptiveChargingToggleEl);
        adaptiveChargingToggle = adaptiveChargingToggleEl;

        const batterySaverToggleEl =
            powerPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#batterySaverToggle');
        assertTrue(!!batterySaverToggleEl);
        batterySaverToggle = batterySaverToggleEl;

        assertEquals(
            1, browserProxy.getCallCount('requestPowerManagementSettings'));
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);
      });

      teardown(() => {
        powerPage.remove();
      });

      test('no battery', async () => {
        await init();
        const batteryStatus = {
          present: false,
          charging: false,
          calculating: false,
          percent: -1,
          statusText: '',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        flush();

        // Power source row is hidden since there's no battery.
        assertTrue(powerSourceRow.hidden);
        // Battery Saver is also hidden.
        assertTrue(batterySaverToggle.hidden);
        // Idle settings while on battery and while charging should not be
        // visible if the battery is not present.
        assertNull(
            powerPage.shadowRoot!.querySelector('#batteryIdleSettingBox'));
        assertNull(powerPage.shadowRoot!.querySelector('#acIdleSettingBox'));

        const noBatteryAcIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#noBatteryAcIdleSelect');
        // Expect the "When idle" dropdown options to appear instead.
        assertTrue(!!noBatteryAcIdleSelect);

        // Select a "When idle" selection and expect it to be set.
        selectValue(noBatteryAcIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.acIdleBehavior);
      });

      test('power sources', () => {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false, false);
        flush();

        // Power sources row is visible but dropdown is hidden.
        assertFalse(powerSourceRow.hidden);
        assertTrue(powerSourceSelect.hidden);

        // Attach a dual-role USB device.
        const powerSource = {
          id: '2',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false, false);
        flush();

        // "Battery" should be selected.
        assertFalse(powerSourceSelect.hidden);
        assertEquals('', powerSourceSelect.value);

        // Select the power source.
        setPowerSources([powerSource], powerSource.id, true, false);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);

        // Send another power source; the first should still be selected.
        const otherPowerSource = Object.assign({}, powerSource);
        otherPowerSource.id = '3';
        setPowerSources(
            [otherPowerSource, powerSource], powerSource.id, true, false);
        flush();
        assertFalse(powerSourceSelect.hidden);
        assertEquals(powerSource.id, powerSourceSelect.value);
      });

      test('choose power source', () => {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));

        // Attach a dual-role USB device.
        const powerSource = {
          id: '3',
          is_dedicated_charger: false,
          description: 'USB-C device',
        };
        setPowerSources([powerSource], '', false, false);
        flush();

        // Select the device.
        selectValue(
            powerSourceSelect,
            (powerSourceSelect.children[1]! as HTMLOptionElement).value);
        assertEquals(powerSource.id, browserProxy.powerSourceId);
      });

      test('set AC idle behavior', () => {
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };
        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        setPowerSources([], '', false, false);
        flush();

        const acIdleSelectElement =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#acIdleSelect');
        assertTrue(!!acIdleSelectElement);
        acIdleSelect = acIdleSelectElement;
        selectValue(acIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.acIdleBehavior);
      });

      test('set battery idle behavior', async () => {
        await new Promise((resolve) => {
          // Indicate battery presence so that idle settings box while
          // on battery is visible.
          const batteryStatus = {
            present: true,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: '5 hours left',
          };
          webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));
          microTask.run(resolve);
        });
        const batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        selectValue(batteryIdleSelect, IdleBehavior.DISPLAY_ON);
        assertEquals(IdleBehavior.DISPLAY_ON, browserProxy.batteryIdleBehavior);
      });

      test('set lid behavior', () => {
        const sendLid = (lidBehavior: LidClosedBehavior) => {
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
              false /* acIdleManaged */, false /* batteryIdleManaged */,
              lidBehavior, false /* lidClosedControlled */, true /* hasLid */,
              false /* adaptiveCharging */, false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
        };

        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);

        let button =
            lidClosedToggle.shadowRoot!.querySelector<HTMLButtonElement>(
                '#control');
        assertTrue(!!button);
        button.click();
        assertEquals(
            LidClosedBehavior.DO_NOTHING, browserProxy.lidClosedBehavior);
        sendLid(LidClosedBehavior.DO_NOTHING);
        assertFalse(lidClosedToggle.checked);

        button = lidClosedToggle.shadowRoot!.querySelector<HTMLButtonElement>(
            '#control');
        assertTrue(!!button);
        button.click();
        assertEquals(LidClosedBehavior.SUSPEND, browserProxy.lidClosedBehavior);
        sendLid(LidClosedBehavior.SUSPEND);
        assertTrue(lidClosedToggle.checked);
      });

      test('display idle behavior for shut_down/stop_session', async () => {
        await new Promise((resolve) => {
          // Send power management settings first.
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
                IdleBehavior.SHUT_DOWN,
                IdleBehavior.STOP_SESSION,
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
                IdleBehavior.SHUT_DOWN,
                IdleBehavior.STOP_SESSION,
              ],
              IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
              true /* acIdleManaged */, true /* batteryIdleManaged */,
              LidClosedBehavior.DO_NOTHING, false /* lidClosedControlled */,
              true /* hasLid */, false /* adaptiveCharging */,
              false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
          microTask.run(resolve);
        });

        // Indicate battery presence so that battery idle settings
        // box becomes visible. Default option should be selected
        // properly even when battery idle settings box is stamped
        // later.
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };

        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        await new Promise((resolve) => {
          microTask.run(resolve);
        });

        let batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(
            IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
        assertFalse(batteryIdleSelect.disabled);
        let acIdleSelectElement =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#acIdleSelect');
        assertTrue(!!acIdleSelectElement);
        acIdleSelect = acIdleSelectElement;
        assertEquals(IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
        assertFalse(acIdleSelect.disabled);
        assertEquals(
            loadTimeData.getString('powerLidSleepLabel'),
            lidClosedToggle.label);
        assertFalse(lidClosedToggle.checked);
        assertFalse(lidClosedToggle.isPrefEnforced());
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
              IdleBehavior.SHUT_DOWN,
              IdleBehavior.STOP_SESSION,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
              IdleBehavior.SHUT_DOWN,
              IdleBehavior.STOP_SESSION,
            ],
            IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
            true /* acIdleManaged */, true /* batteryIdleManaged */,
            LidClosedBehavior.DO_NOTHING, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);

        await new Promise((resolve) => microTask.run(resolve));

        batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(
            IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
        assertFalse(batteryIdleSelect.disabled);
        acIdleSelectElement =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#acIdleSelect');
        assertTrue(!!acIdleSelectElement);
        acIdleSelect = acIdleSelectElement;
        assertEquals(IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
        assertFalse(acIdleSelect.disabled);
        assertEquals(
            loadTimeData.getString('powerLidSleepLabel'),
            lidClosedToggle.label);
        assertFalse(lidClosedToggle.checked);
        assertFalse(lidClosedToggle.isPrefEnforced());
      });

      test('display idle and lid behavior', async () => {
        await new Promise((resolve) => {
          // Send power management settings first.
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              IdleBehavior.DISPLAY_ON, IdleBehavior.DISPLAY_OFF,
              false /* acIdleManaged */, false /* batteryIdleManaged */,
              LidClosedBehavior.DO_NOTHING, false /* lidClosedControlled */,
              true /* hasLid */, false /* adaptiveCharging */,
              false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
          microTask.run(resolve);
        });

        // Indicate battery presence so that battery idle settings
        // box becomes visible. Default option should be selected
        // properly even when battery idle settings box is stamped
        // later.
        const batteryStatus = {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        };

        webUIListenerCallback(
            'battery-status-changed', Object.assign({}, batteryStatus));
        await new Promise((resolve) => microTask.run(resolve));

        const acIdleSelectElement =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#acIdleSelect');
        assertTrue(!!acIdleSelectElement);
        acIdleSelect = acIdleSelectElement;
        let batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(IdleBehavior.DISPLAY_ON.toString(), acIdleSelect.value);
        assertEquals(
            IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
        assertFalse(acIdleSelect.disabled);
        assertNull(
            powerPage.shadowRoot!.querySelector('#acIdleManagedIndicator'));
        assertEquals(
            loadTimeData.getString('powerLidSleepLabel'),
            lidClosedToggle.label);
        assertFalse(lidClosedToggle.checked);
        assertFalse(lidClosedToggle.isPrefEnforced());
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_ON,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);

        await new Promise((resolve) => microTask.run(resolve));

        batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
        assertEquals(
            IdleBehavior.DISPLAY_ON.toString(), batteryIdleSelect.value);
        assertFalse(acIdleSelect.disabled);
        assertFalse(batteryIdleSelect.disabled);
        assertNull(
            powerPage.shadowRoot!.querySelector('#acIdleManagedIndicator'));
        assertNull(powerPage.shadowRoot!.querySelector(
            '#batteryIdleManagedIndicator'));
        assertEquals(
            loadTimeData.getString('powerLidSleepLabel'),
            lidClosedToggle.label);
        assertTrue(lidClosedToggle.checked);
        assertFalse(lidClosedToggle.isPrefEnforced());
      });

      test('display managed idle and lid behavior', async () => {
        // When settings are managed, the controls should be disabled and
        // the indicators should be shown.
        await new Promise((resolve) => {
          // Indicate battery presence so that idle settings box while
          // on battery is visible.
          const batteryStatus = {
            present: true,
            charging: false,
            calculating: false,
            percent: 50,
            statusText: '5 hours left',
          };
          webUIListenerCallback(
              'battery-status-changed', Object.assign({}, batteryStatus));
          sendPowerManagementSettings(
              [IdleBehavior.SHUT_DOWN], [IdleBehavior.SHUT_DOWN],
              IdleBehavior.SHUT_DOWN, IdleBehavior.SHUT_DOWN,
              true /* acIdleManaged */, true /* batteryIdleManaged */,
              LidClosedBehavior.SHUT_DOWN, true /* lidClosedControlled */,
              true /* hasLid */, false /* adaptiveCharging */,
              false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
          microTask.run(resolve);
        });

        const acIdleSelectElement =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#acIdleSelect');
        assertTrue(!!acIdleSelectElement);
        acIdleSelect = acIdleSelectElement;
        let batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(IdleBehavior.SHUT_DOWN.toString(), acIdleSelect.value);
        assertEquals(
            IdleBehavior.SHUT_DOWN.toString(), batteryIdleSelect.value);
        assertTrue(acIdleSelect.disabled);
        assertTrue(batteryIdleSelect.disabled);
        assertTrue(
            !!powerPage.shadowRoot!.querySelector('#acIdleManagedIndicator'));
        assertTrue(!!powerPage.shadowRoot!.querySelector(
            '#batteryIdleManagedIndicator'));
        assertEquals(
            loadTimeData.getString('powerLidShutDownLabel'),
            lidClosedToggle.label);
        assertTrue(lidClosedToggle.checked);
        assertTrue(lidClosedToggle.isPrefEnforced());
        sendPowerManagementSettings(
            [IdleBehavior.DISPLAY_OFF], [IdleBehavior.DISPLAY_OFF],
            IdleBehavior.DISPLAY_OFF, IdleBehavior.DISPLAY_OFF,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.STOP_SESSION, true /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);

        await new Promise((resolve) => microTask.run(resolve));

        batteryIdleSelect =
            powerPage.shadowRoot!.querySelector<HTMLSelectElement>(
                '#batteryIdleSelect');
        assertTrue(!!batteryIdleSelect);
        assertEquals(IdleBehavior.DISPLAY_OFF.toString(), acIdleSelect.value);
        assertEquals(
            IdleBehavior.DISPLAY_OFF.toString(), batteryIdleSelect.value);
        assertTrue(acIdleSelect.disabled);
        assertTrue(batteryIdleSelect.disabled);
        assertNull(
            powerPage.shadowRoot!.querySelector('#acIdleManagedIndicator'));
        assertNull(powerPage.shadowRoot!.querySelector(
            '#batteryIdleManagedIndicator'));
        assertEquals(
            loadTimeData.getString('powerLidSignOutLabel'),
            lidClosedToggle.label);
        assertTrue(lidClosedToggle.checked);
        assertTrue(lidClosedToggle.isPrefEnforced());
      });

      test('hide lid behavior when lid not present', async () => {
        await new Promise((resolve) => {
          const toggle =
              powerPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#lidClosedToggle');
          assertTrue(!!toggle);
          assertFalse(toggle.hidden);
          sendPowerManagementSettings(
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              [
                IdleBehavior.DISPLAY_OFF_SLEEP,
                IdleBehavior.DISPLAY_OFF,
                IdleBehavior.DISPLAY_ON,
              ],
              IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
              false /* acIdleManaged */, false /* batteryIdleManaged */,
              LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
              false /* hasLid */, false /* adaptiveCharging */,
              false /* adaptiveChargingManaged */,
              true /* batterySaverFeatureEnabled */);
          microTask.run(resolve);
        });

        const toggle1 =
            powerPage.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                '#lidClosedToggle');
        assertTrue(!!toggle1);
        assertTrue(toggle1.hidden);
      });

      test(
          'hide display controlled battery idle behavior when battery not present',
          async () => {
            await new Promise((resolve) => {
              const batteryStatus = {
                present: false,
                charging: false,
                calculating: false,
                percent: -1,
                statusText: '',
              };
              webUIListenerCallback(
                  'battery-status-changed', Object.assign({}, batteryStatus));
              flush();
              microTask.run(resolve);
            });
            assertNull(
                powerPage.shadowRoot!.querySelector('#batteryIdleSettingBox'));
          });

      test('Deep link to sleep when laptop lid closed', async () => {
        const crToggle = lidClosedToggle.shadowRoot!.querySelector('cr-toggle');
        assertTrue(!!crToggle);
        await checkDeepLink(
            routes.POWER,
            settingMojom.Setting.kSleepWhenLaptopLidClosed.toString(), crToggle,
            'Sleep when closed toggle');
      });

      test('Adaptive charging controlled by policy', () => {
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, true /* adaptiveCharging */,
            true /* adaptiveCharingManaged */,
            true /* batterySaverFeatureEnabled */);

        const crToggle =
            adaptiveChargingToggle.shadowRoot!.querySelector('cr-toggle');
        assertTrue(!!crToggle);
        assertTrue(crToggle.checked);

        // Must have policy icon.
        assertTrue(isVisible(adaptiveChargingToggle.shadowRoot!.querySelector(
            'cr-policy-pref-indicator')));

        // Must have toggle locked.
        assertTrue(crToggle.disabled);
      });

      test('Deep link to adaptive charging', async () => {
        const crToggle =
            adaptiveChargingToggle.shadowRoot!.querySelector('cr-toggle');
        assertTrue(!!crToggle);
        await checkDeepLink(
            routes.POWER, settingMojom.Setting.kAdaptiveCharging.toString(),
            crToggle, 'Adaptive charging toggle');
      });

      test('Battery Saver hidden when feature disabled', () => {
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            false /* batterySaverFeatureEnabled */);

        assertTrue(batterySaverToggle.hidden);
      });

      test('Battery Saver toggleable', () => {
        // Battery is present.
        webUIListenerCallback('battery-status-changed', {
          present: true,
          charging: false,
          calculating: false,
          percent: 50,
          statusText: '5 hours left',
        });

        // There are no power sources.
        setPowerSources([], '', false, false);

        // Battery saver feature is enabled.
        sendPowerManagementSettings(
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            [
              IdleBehavior.DISPLAY_OFF_SLEEP,
              IdleBehavior.DISPLAY_OFF,
              IdleBehavior.DISPLAY_ON,
            ],
            IdleBehavior.DISPLAY_OFF_SLEEP, IdleBehavior.DISPLAY_OFF_SLEEP,
            false /* acIdleManaged */, false /* batteryIdleManaged */,
            LidClosedBehavior.SUSPEND, false /* lidClosedControlled */,
            true /* hasLid */, false /* adaptiveCharging */,
            false /* adaptiveChargingManaged */,
            true /* batterySaverFeatureEnabled */);

        // Battery saver should be visible and toggleable.
        assertFalse(batterySaverToggle.hidden);
        assertFalse(batterySaverToggle.disabled);

        // Connect a dedicated AC power adapter.
        const mainsPowerSource = {
          id: '1',
          is_dedicated_charger: true,
          description: 'USB-C device',
        };
        setPowerSources([mainsPowerSource], '1', false, true);

        // Battery saver should be visible but not toggleable.
        assertFalse(batterySaverToggle.hidden);
        assertTrue(batterySaverToggle.disabled);
      });

      test('Battery Saver updates when pref updates', () => {
        function setPref(value: boolean): void {
          const newPrefs = getFakePrefs();
          newPrefs.power.cros_battery_saver_active.value = value;
          powerPage.prefs = newPrefs;
          flush();
        }

        setPref(true);
        assertTrue(batterySaverToggle.checked);

        setPref(false);
        assertFalse(batterySaverToggle.checked);
      });
    });
  });
});
