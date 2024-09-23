// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CupsPrintersBrowserProxyImpl, PrinterSettingsUserAction, PrinterType, SettingsCupsPrintersElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {createCupsPrinterInfo, createPrinterListEntry} from './cups_printer_test_utils.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

suite('<settings-cups-printers>', () => {
  let page: SettingsCupsPrintersElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;
  let wifi: NetworkStateProperties;

  async function resetPage(): Promise<void> {
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    await flushTasks();

    // Simulate internet connection.
    wifi = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi');
    wifi.connectionState = ConnectionStateType.kOnline;
    page.onActiveNetworksChanged([wifi]);
    return Promise.resolve();
  }

  setup(async () => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    await resetPage();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
  });

  // Verify the Saved printers section strings.
  test('SavedPrintersText', async () => {
    page.set('savedPrinters_', [createPrinterListEntry(
                                   'nameA', 'printerAddress', 'idA',
                                   PrinterType.SAVED)]);

    await flushTasks();
    const container =
        page.shadowRoot!.querySelector('#savedPrintersContainer .secondary');
    assertTrue(!!container);
    assertEquals(
        loadTimeData.getString('savedPrintersSubtext'),
        container.textContent?.trim());
  });

  // Verify the Nearby printers section strings.
  test('AvailablePrintersText', () => {
    const availablePrintersReadyTitle =
        page.shadowRoot!.querySelector('#availablePrintersReadyTitle');
    assertTrue(!!availablePrintersReadyTitle);
    assertEquals(
        loadTimeData.getString('availablePrintersReadyTitle'),
        availablePrintersReadyTitle.textContent?.trim());
    const availablePrintersReadySubtext =
        page.shadowRoot!.querySelector('#availablePrintersReadySubtext');
    assertTrue(!!availablePrintersReadySubtext);
    assertEquals(
        loadTimeData.getString('availablePrintersReadySubtext'),
        availablePrintersReadySubtext.textContent?.trim());
  });

  // Verify the Nearby printers section can be properly opened and closed.
  test('CollapsibleNearbyPrinterSection', () => {
    // The collapsible section should start opened, then after clicking the
    // button should close.
    const toggleButton = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#nearbyPrinterToggleButton');
    assertTrue(!!toggleButton);
    assertTrue(
        isVisible(page.shadowRoot!.querySelector('#collapsibleSection')));
    assertTrue(isVisible(page.shadowRoot!.querySelector('#helpSection')));
    toggleButton.click();
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#collapsibleSection')));
    assertFalse(isVisible(page.shadowRoot!.querySelector('#helpSection')));
    toggleButton.click();
    assertTrue(
        isVisible(page.shadowRoot!.querySelector('#collapsibleSection')));
    assertTrue(isVisible(page.shadowRoot!.querySelector('#helpSection')));
  });

  // Verify the Saved printers empty state only shows when there are no saved
  // printers.
  test('SavedPrintersEmptyState', async () => {
    // Settings should start in empty state without saved printers.
    const emptyState = page.shadowRoot!.querySelector('#noSavedPrinters');
    assertTrue(isVisible(emptyState));

    await flushTasks();

    // Add a saved printer and expect the empty state to be hidden.
    webUIListenerCallback('on-saved-printers-changed', {
      printerList: [
        createCupsPrinterInfo('nameA', 'address', 'idA'),
      ],
    });
    assertFalse(isVisible(emptyState));
  });

  // Verify the Nearby printers section starts open when there are no saved
  // printers or open when there's more than one saved printer.
  test('CollapsibleNearbyPrinterSectionSavedPrinters', async () => {
    // Simulate no saved printers and expect the section to be open.
    cupsPrintersBrowserProxy.printerList = {printerList: []};
    resetPage();
    await flushTasks();
    assertTrue(
        isVisible(page.shadowRoot!.querySelector('#collapsibleSection')));

    // Simulate 1 saved printer on load and expect the section to be
    // collapsed.
    cupsPrintersBrowserProxy.printerList = {
      printerList: [
        createCupsPrinterInfo('nameA', 'address', 'idA'),
      ],
    };
    resetPage();
    await flushTasks();
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#collapsibleSection')));
  });

  // Verify clicking the add printer manually button is recorded to metrics.
  test('RecordUserActionMetric', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    // Enable the add printer manually button.
    page.prefs = {
      native_printing: {
        user_native_printers_allowed: {
          value: true,
        },
      },
    };

    await flushTasks();
    const icon = page.shadowRoot!.querySelector<HTMLButtonElement>(
        '#addManualPrinterButton');
    assertTrue(!!icon);
    icon.click();
    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.ADD_PRINTER_MANUALLY));
  });

  /**
   * Test that available printers section is hidden when offline.
   */
  test('OfflineHideAvailablePrinters', async () => {
    assertFalse(isVisible(
        page.shadowRoot!.querySelector('#noConnectivityContentContainer')));

    wifi.connectionState = ConnectionStateType.kNotConnected;
    page.onActiveNetworksChanged([wifi]);
    flush();

    assertTrue(isVisible(
        page.shadowRoot!.querySelector('#noConnectivityContentContainer')));
  });
});

suite('with isRevampWayfindingEnabled set to true', () => {
  let page: SettingsCupsPrintersElement;
  setup(() => {
    loadTimeData.overrideValues({
      isRevampWayfindingEnabled: true,
    });

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);

    flush();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
    page.remove();
  });

  test('Deep link to print jobs', async () => {
    const params = new URLSearchParams();
    const printJobsSettingId = settingMojom.Setting.kPrintJobs.toString();
    params.append('settingId', printJobsSettingId);
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector<HTMLElement>('#printManagement');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, page.shadowRoot!.activeElement,
        `Print jobs button should be focused for settingId=${
            printJobsSettingId}.`);
  });
});
