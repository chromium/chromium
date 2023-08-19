// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {DiagnosticsAppElement} from 'chrome://diagnostics/diagnostics_app.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakeNetworkGuidInfoList, fakeSystemInfo, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {isVisible} from '../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('appTestSuite', function() {
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
    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);

    // Setup a fake routine controller.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes.emptyHTML;
  });

  teardown(() => {
    page.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  /**
   * @param {boolean} isLoggedIn
   * @suppress {visibility} // access private member
   * @return {!Promise}
   */
  function changeLoggedInState(isLoggedIn) {
    page.isLoggedIn = isLoggedIn;
    return flushTasks();
  }

  /**
   * Simulate clicking session log button.
   * @return {!Promise}
   */
  function clickSessionLogButton() {
    const sessionLogButton = getSessionLogButton();
    assertTrue(!!sessionLogButton);
    sessionLogButton.click();

    return flushTasks();
  }

  /** @return {!HTMLElement} */
  function getCautionBanner() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (
        page.shadowRoot.querySelector('diagnostics-sticky-banner'));
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

    return /** @type {!CrButtonElement} */ (
        page.shadowRoot.querySelector('.session-log-button'));
  }

  /**
   * @return {!HTMLElement}
   */
  function getBottomNavContentDrawer() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (
        page.shadowRoot.querySelector('[slot=bottom-nav-content-drawer]'));
  }

  /**
   * @return {!HTMLElement}
   */
  function getBottomNavContentPanel() {
    assertTrue(!!page);

    return /** @type {!HTMLElement} */ (
        page.shadowRoot.querySelector('[slot=bottom-nav-content-panel]'));
  }

  /**
   * Returns whether the toast is visible or not.
   * @return {boolean}
   */
  function isToastVisible() {
    return page.shadowRoot.querySelector('cr-toast').open;
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

  test('IsJellyEnabledForDiagnosticsApp', async () => {
    // Setup test for jelly disabled.
    loadTimeData.overrideValues({
      isJellyEnabledForDiagnosticsApp: false,
    });
    /*@type {HTMLLinkElement}*/
    const link = document.createElement('link');
    const disabledUrl = 'chrome://resources/chromeos/colors/cros_styles.css';
    link.href = disabledUrl;
    document.head.appendChild(link);
    await initializeDiagnosticsApp(
        fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
        fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage);

    dx_utils.assertTextContains(link.href, disabledUrl);

    // Reset diagnostics app element
    document.body.innerHTML = window.trustedTypes.emptyHTML;
    page.remove();
    page = null;

    // Setup test for jelly enabled.
    loadTimeData.overrideValues({
      isJellyEnabledForDiagnosticsApp: true,
    });
    await initializeDiagnosticsApp(
        fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
        fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage);

    const enabledUrl = 'chrome://theme/colors.css?sets=legacy';
    dx_utils.assertTextContains(link.href, enabledUrl);

    // Clean up test specific element.
    document.head.removeChild(link);
  });

  test('SaveSessionLogDisabledWhenPendingResult', () => {
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

  test('SaveSessionLogSuccessShowsToast', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(true);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.shadowRoot.querySelector('#toast'),
                loadTimeData.getString('sessionLogToastTextSuccess'));
          });
        });
  });

  test('SaveSessionLogFailure', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => {
          DiagnosticsBrowserProxy.setSuccess(false);
          clickSessionLogButton().then(() => {
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
                page.shadowRoot.querySelector('#toast'),
                loadTimeData.getString('sessionLogToastTextFailure'));
          });
        });
  });

  test('SessionLogHiddenWhenNotLoggedIn', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (false)))
        .then(() => assertFalse(isVisible(getSessionLogButton())));
  });

  test('SessionLogShownWhenLoggedIn', () => {
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => changeLoggedInState(/* isLoggedIn */ (true)))
        .then(() => assertTrue(isVisible(getSessionLogButton())));
  });
});
