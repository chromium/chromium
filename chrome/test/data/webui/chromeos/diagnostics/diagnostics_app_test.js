// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';

import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakeNetworkGuidInfoList, fakePowerRoutineResults, fakeRoutineResults, fakeSystemInfo, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

export function appTestSuite() {
  /** @type {?DiagnosticsAppElement} */
  let page = null;

  /** @type {?FakeSystemDataProvider} */
  let systemDataProvider = null;

  /** @type {?FakeNetworkHealthProvider} */
  let networkHealthProvider = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {?TestDiagnosticsBrowserProxy} */
  let DiagnosticsBrowserProxy = null;

  suiteSetup(() => {
    systemDataProvider = new FakeSystemDataProvider();
    networkHealthProvider = new FakeNetworkHealthProvider();

    setSystemDataProviderForTesting(systemDataProvider);
    setNetworkHealthProviderForTesting(networkHealthProvider);

    DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();
    DiagnosticsBrowserProxyImpl.instance_ = DiagnosticsBrowserProxy;

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    page.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  /** @return {!HTMLElement} */
  function getCautionBanner() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (page.$$('diagnostics-sticky-banner'));
  }

  /** @return {!HTMLElement} */
  function getCautionBannerMessage() {
    return /** @type {!HTMLElement} */ (
        getCautionBanner().shadowRoot.querySelector('#bannerMsg'));
  }

  /**
   * @return {!CrButtonElement}
   */
  function getSessionLogButton() {
    assertTrue(!!page);

    return /** @type {!CrButtonElement} */ (page.$$('.session-log-button'));
  }

  /**
   * @return {!HTMLElement}
   */
  function getBottomNavContentDrawer() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (
        page.$$('[slot=bottom-nav-content-drawer]'));
  }

  /**
   * @return {!HTMLElement}
   */
  function getBottomNavContentPanel() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (
        page.$$('[slot=bottom-nav-content-panel]'));
  }

  /**
   * Triggers 'dismiss-caution-banner' custom event.
   * @return {!Promise}
   */
  function triggerDismissBannerEvent() {
    window.dispatchEvent(new CustomEvent('dismiss-caution-banner', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'show-caution-banner' custom event with correctly configured event
   * detail object based on provided message.
   * @param {string} message
   * @return {!Promise}
   */
  function triggerShowBannerEvent(message) {
    window.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message},
    }));

    return flushTasks();
  }

  /**
   * @param {!SystemInfo} systemInfo
   * @param {!Array<!BatteryChargeStatus>} batteryChargeStatus
   * @param {!Array<!BatteryHealth>} batteryHealth
   * @param {!BatteryInfo} batteryInfo
   * @param {!Array<!CpuUsage>} cpuUsage
   * @param {!Array<!MemoryUsage>} memoryUsage
   */
  function initializeDiagnosticsApp(
      systemInfo, batteryChargeStatus, batteryHealth, batteryInfo, cpuUsage,
      memoryUsage) {
    assertFalse(!!page);

    // Initialize the fake data.
    systemDataProvider.setFakeSystemInfo(systemInfo);
    systemDataProvider.setFakeBatteryChargeStatus(batteryChargeStatus);
    systemDataProvider.setFakeBatteryHealth(batteryHealth);
    systemDataProvider.setFakeBatteryInfo(batteryInfo);
    systemDataProvider.setFakeCpuUsage(cpuUsage);
    systemDataProvider.setFakeMemoryUsage(memoryUsage);

    networkHealthProvider.setFakeNetworkGuidInfo(fakeNetworkGuidInfoList);
    networkHealthProvider.setFakeNetworkState(
        'ethernetGuid', [fakeEthernetNetwork]);
    networkHealthProvider.setFakeNetworkState('wifiGuid', [fakeWifiNetwork]);
    networkHealthProvider.setFakeNetworkState(
        'cellularGuid', [fakeCellularNetwork]);

    page = /** @type {!DiagnosticsAppElement} */ (
        document.createElement('diagnostics-app'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  if (window.isNetworkEnabled || window.isInputEnabled) {
    test('SystemPagePopulated', () => {
      return initializeDiagnosticsApp(
                 fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
                 fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
          .then(() => {
            const systemPage =
                dx_utils.getNavigationViewPanelElement(page, 'system');
            assertTrue(!!systemPage);
            assertTrue(isVisible(systemPage));
            assertFalse(isVisible(getCautionBanner()));
            assertFalse(isVisible(getBottomNavContentDrawer()));
            assertTrue(isVisible(getBottomNavContentPanel()));
            assertTrue(isVisible(getSessionLogButton()));
          });
    });

    test('BannerVisibliblityTogglesWithEvents', () => {
      const bannerMessage = 'Diagnostics Banner Message';
      return initializeDiagnosticsApp(
                 fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
                 fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
          .then(() => {
            assertFalse(isVisible(getCautionBanner()));

            return triggerShowBannerEvent(bannerMessage);
          })
          .then(() => {
            assertTrue(isVisible(getCautionBanner()));
            dx_utils.assertElementContainsText(
                getCautionBannerMessage(), bannerMessage);

            return triggerDismissBannerEvent();
          })
          .then(() => assertFalse(isVisible(getCautionBanner())));
    });

    test('SaveSessionLogDisabledUntilResolved', () => {
      return initializeDiagnosticsApp(
                 fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
                 fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
          .then(() => {
            assertFalse(getSessionLogButton().disabled);

            DiagnosticsBrowserProxy.setSuccess(true);
            getSessionLogButton().click();
            assertTrue(getSessionLogButton().disabled);

            return flushTasks();
          })
          .then(() => {
            assertFalse(getSessionLogButton().disabled);
          });
    });
  }
}
