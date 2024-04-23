// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/diagnostics_app.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {DiagnosticsAppElement} from 'chrome://diagnostics/diagnostics_app.js';
import {DiagnosticsBrowserProxyImpl} from 'chrome://diagnostics/diagnostics_browser_proxy.js';
import {DiagnosticsStickyBannerElement} from 'chrome://diagnostics/diagnostics_sticky_banner.js';
import {fakeBatteryChargeStatus, fakeBatteryHealth, fakeBatteryInfo, fakeCellularNetwork, fakeCpuUsage, fakeEthernetNetwork, fakeMemoryUsage, fakeNetworkGuidInfoList, fakeSystemInfo, fakeWifiNetwork} from 'chrome://diagnostics/fake_data.js';
import {FakeNetworkHealthProvider} from 'chrome://diagnostics/fake_network_health_provider.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setNetworkHealthProviderForTesting, setSystemDataProviderForTesting, setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {BatteryChargeStatus, BatteryHealth, BatteryInfo, CpuUsage, MemoryUsage, SystemInfo} from 'chrome://diagnostics/system_data_provider.mojom-webui.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';
import {TestDiagnosticsBrowserProxy} from './test_diagnostics_browser_proxy.js';

suite('appTestSuite', function() {
  let page: DiagnosticsAppElement|null = null;

  const systemDataProvider = new FakeSystemDataProvider();

  const networkHealthProvider = new FakeNetworkHealthProvider();

  const routineController = new FakeSystemRoutineController();

  const DiagnosticsBrowserProxy = new TestDiagnosticsBrowserProxy();

  suiteSetup(() => {
    setSystemDataProviderForTesting(systemDataProvider);
    setNetworkHealthProviderForTesting(networkHealthProvider);

    DiagnosticsBrowserProxyImpl.setInstance(DiagnosticsBrowserProxy);

    // Setup a fake routine controller.
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        routineController.getAllRoutines());

    setSystemRoutineControllerForTesting(routineController);
  });

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  teardown(() => {
    page?.remove();
    page = null;
    systemDataProvider.reset();
    networkHealthProvider.reset();
  });

  /**
   * Simulate clicking session log button.
   */
  function clickSessionLogButton(): Promise<void> {
    const sessionLogButton = getSessionLogButton();
    assert(sessionLogButton);
    sessionLogButton.click();

    return flushTasks();
  }

  function getCautionBanner(): DiagnosticsStickyBannerElement {
    assert(page);
    return strictQuery(
        'diagnostics-sticky-banner', page.shadowRoot,
        DiagnosticsStickyBannerElement);
  }

  function getCautionBannerMessage(): HTMLElement {
    return strictQuery(
        '#bannerMsg', getCautionBanner().shadowRoot, HTMLElement);
  }

  function getSessionLogButton(): CrButtonElement {
    assert(page);
    return strictQuery('.session-log-button', page.shadowRoot, CrButtonElement);
  }

  function getBottomNavContentDrawer(): HTMLElement {
    assert(page);
    return strictQuery(
        '[slot=bottom-nav-content-drawer]', page.shadowRoot, HTMLElement);
  }

  function getBottomNavContentPanel(): HTMLElement {
    assert(page);
    return strictQuery(
        '[slot=bottom-nav-content-panel]', page.shadowRoot, HTMLElement);
  }

  /**
   * Returns whether the toast is visible or not.
   */
  function isToastVisible(): boolean {
    assert(page);
    return strictQuery('cr-toast', page.shadowRoot, CrToastElement).open;
  }

  /**
   * Triggers 'dismiss-caution-banner' custom event.
   */
  function triggerDismissBannerEvent(): Promise<void> {
    window.dispatchEvent(new CustomEvent('dismiss-caution-banner', {
      bubbles: true,
      composed: true,
    }));

    return flushTasks();
  }

  /**
   * Triggers 'show-caution-banner' custom event with correctly configured event
   * detail object based on provided message.
   */
  function triggerShowBannerEvent(message: string): Promise<void> {
    window.dispatchEvent(new CustomEvent('show-caution-banner', {
      bubbles: true,
      composed: true,
      detail: {message},
    }));

    return flushTasks();
  }

  function initializeDiagnosticsApp(
      systemInfo: SystemInfo, batteryChargeStatus: BatteryChargeStatus[],
      batteryHealth: BatteryHealth[], batteryInfo: BatteryInfo,
      cpuUsage: CpuUsage[], memoryUsage: MemoryUsage[]): Promise<void> {
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

    page = document.createElement('diagnostics-app');
    assert(page);
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
            assert(page);
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
              strictQuery('#toast', page.shadowRoot, CrToastElement),
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
            assert(page);
            assertTrue(isToastVisible());
            dx_utils.assertElementContainsText(
              strictQuery('#toast', page.shadowRoot, CrToastElement),
                loadTimeData.getString('sessionLogToastTextFailure'));
          });
        });
  });

  // TODO(b/336491585): Test consistently failing on ChromeOS.
  test.skip('SessionLogHiddenWhenNotLoggedIn', () => {
    loadTimeData.overrideValues({isLoggedIn: false});
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => assertFalse(isVisible(getSessionLogButton())));
  });

  test('SessionLogShownWhenLoggedIn', () => {
    loadTimeData.overrideValues({isLoggedIn: true});
    return initializeDiagnosticsApp(
               fakeSystemInfo, fakeBatteryChargeStatus, fakeBatteryHealth,
               fakeBatteryInfo, fakeCpuUsage, fakeMemoryUsage)
        .then(() => assertTrue(isVisible(getSessionLogButton())));
  });
});
