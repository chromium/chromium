// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AddPrintServerDialogElement, CupsPrinterInfo, CupsPrintersBrowserProxyImpl, CupsPrintersEntryManager, PrinterDialogErrorElement, PrinterListEntry, PrinterType, PrintServerResult, SettingsCupsAddPrinterDialogElement, SettingsCupsPrintersElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrToastElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {createCupsPrinterInfo, createPrinterListEntry} from './cups_printer_test_utils.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

suite('PrintServerTests', () => {
  let page: SettingsCupsPrintersElement;
  let dialog: SettingsCupsAddPrinterDialogElement;
  let entryManager: CupsPrintersEntryManager;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;

  setup(async () => {
    entryManager = CupsPrintersEntryManager.getInstance();
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*automaticPrinters=*/[],
        /*discoveredPrinters=*/[], /*printServerPrinters=*/[]);

    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    const element =
        page.shadowRoot!.querySelector('settings-cups-add-printer-dialog');
    assertTrue(!!element);
    dialog = element;

    await flushTasks();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    dialog.remove();
  });

  function setEntryManagerPrinters(
      savedPrinters: PrinterListEntry[], automaticPrinters: CupsPrinterInfo[],
      discoveredPrinters: CupsPrinterInfo[],
      printerServerPrinters: PrinterListEntry[]): void {
    entryManager.setSavedPrintersList(savedPrinters);
    entryManager.setNearbyPrintersList(automaticPrinters, discoveredPrinters);
    entryManager.printServerPrinters = printerServerPrinters;
  }

  /**
   * Returns the print server dialog if it is available.
   */
  function getPrintServerDialog(page: SettingsCupsPrintersElement):
      AddPrintServerDialogElement {
    assertTrue(!!page);
    const element =
        page.shadowRoot!.querySelector('settings-cups-add-printer-dialog');
    assertTrue(!!element);
    dialog = element;
    const addDialog =
        dialog.shadowRoot!.querySelector('add-print-server-dialog');
    assertTrue(!!addDialog);
    return addDialog;
  }

  /**
   * Opens the add print server dialog, inputs |address| with the specified
   * |error|. Adds the print server and returns a promise for handling the add
   * event.
   * The promise returned when queryPrintServer is called.
   */
  async function addPrintServer(address: string, error: number): Promise<void> {
    // Open the add manual printe dialog.
    assertTrue(!!page);
    dialog.open();
    flush();

    const addPrinterDialog =
        dialog.shadowRoot!.querySelector('add-printer-manually-dialog');
    assertTrue(!!addPrinterDialog);
    // Switch to Add print server dialog.
    let button = addPrinterDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '#print-server-button');
    assertTrue(!!button);
    button.click();
    flush();
    const printServerDialog =
        dialog.shadowRoot!.querySelector('add-print-server-dialog');
    assertTrue(!!printServerDialog);

    flush();
    cupsPrintersBrowserProxy.setQueryPrintServerResult(error);
    await flushTasks();
    // Fill dialog with the server address.
    const printServerAddressInput =
        printServerDialog.shadowRoot!.querySelector<HTMLInputElement>(
            '#printServerAddressInput');
    assertTrue(!!printServerAddressInput);
    printServerAddressInput.value = address;
    // Add the print server.
    button = printServerDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.action-button');
    assertTrue(!!button);
    // Button should not be disabled before clicking on it.
    assertFalse(button.disabled);
    button.click();
    // Clicking on the button should disable it.
    assertTrue(button.disabled);
    await cupsPrintersBrowserProxy.whenCalled('queryPrintServer');
  }

  function verifyErrorMessage(expectedError: string): void {
    // Assert that the dialog did not close on errors.
    const printServerDialog = getPrintServerDialog(page);
    const dialogError =
        printServerDialog.shadowRoot!.querySelector<PrinterDialogErrorElement>(
            '#server-dialog-error');
    assertTrue(!!dialogError);
    // Assert that the dialog error is displayed.
    assertFalse(dialogError.hidden);
    assertEquals(
        loadTimeData.getString(expectedError), dialogError.get('errorText'));
  }

  function verifyToastMessage(
      expectedMessage: string, numPrinters: number): void {
    // We always display the total number of printers found from a print
    // server.
    const toast = page.shadowRoot!.querySelector<CrToastElement>(
        '#printServerErrorToast');
    assertTrue(!!toast);
    assertTrue(toast.open);
    assertEquals(
        loadTimeData.getStringF(expectedMessage, numPrinters),
        toast.textContent?.trim());
  }

  test('AddPrintServerIsSuccessful', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters = {
      printerList: [
        createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
        createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
      ],
    };
    await addPrintServer('serverAddress', PrintServerResult.NO_ERRORS);
    flush();
    verifyToastMessage('printServerFoundManyPrinters', /*numPrinters=*/ 2);
    assertEquals(2, entryManager.printServerPrinters.length);
  });

  test('HandleDuplicateQueries', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters = {
      printerList: [
        createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
        createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
      ],
    };

    await flushTasks();
    // Simulate that a print server was queried previously.
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*nearbyPrinters=*/[],
        /*discoveredPrinters=*/[], [
          createPrinterListEntry(
              'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
          createPrinterListEntry(
              'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER),
        ]);
    flush();
    assertEquals(2, entryManager.printServerPrinters.length);

    // This will attempt to add duplicate print server printers.
    // Matching printerId's are considered duplicates.
    await addPrintServer('serverAddress', PrintServerResult.NO_ERRORS);
    flush();

    verifyToastMessage('printServerFoundManyPrinters', /*numPrinters=*/ 2);
    // Assert that adding the same print server results in no new printers
    // added to the entry manager.
    assertEquals(2, entryManager.printServerPrinters.length);
    const nearbyPrintersElement =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!nearbyPrintersElement);
    assertEquals(2, nearbyPrintersElement.nearbyPrinters.length);
  });

  test('HandleDuplicateSavedPrinters', async () => {
    // Initialize the return result from adding a print server.
    cupsPrintersBrowserProxy.printServerPrinters = {
      printerList: [
        createCupsPrinterInfo('nameA', 'serverAddress', 'idA'),
        createCupsPrinterInfo('nameB', 'serverAddress', 'idB'),
      ],
    };

    await flushTasks();
    // Simulate that a print server was queried previously.
    setEntryManagerPrinters(
        /*savedPrinters=*/[], /*nearbyPrinters=*/[],
        /*discoveredPrinters=*/[], [
          createPrinterListEntry(
              'nameA', 'serverAddress', 'idA', PrinterType.PRINTSERVER),
          createPrinterListEntry(
              'nameB', 'serverAddress', 'idB', PrinterType.PRINTSERVER),
        ]);
    flush();
    assertEquals(2, entryManager.printServerPrinters.length);

    // Simulate adding a saved printer.
    entryManager.setSavedPrintersList([createPrinterListEntry(
        'nameA', 'serverAddress', 'idA', PrinterType.SAVED)]);
    flush();

    // Simulate the underlying model changes. Nearby printers are also
    // updated after changes to saved printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', /*automaticPrinter=*/[],
        /*discoveredPrinters=*/[]);
    await flushTasks();

    // Verify that we now only have 1 printer in print server printers
    // list.
    assertEquals(1, entryManager.printServerPrinters.length);
    const nearbyPrintersElement =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!nearbyPrintersElement);
    assertEquals(1, nearbyPrintersElement.nearbyPrinters.length);
    // Verify we correctly removed the duplicate printer, 'idA', since
    // it exists in the saved printer list. Expect only 'idB' in
    // the print server printers list.
    assertEquals(
        'idB', entryManager.printServerPrinters[0]!.printerInfo.printerId);
  });

  test('AddPrintServerAddressError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters = {printerList: []};
    await addPrintServer('serverAddress', PrintServerResult.INCORRECT_URL);
    flush();
    const printServerDialog = getPrintServerDialog(page);
    // Assert that the dialog did not close on errors.
    assertTrue(!!printServerDialog);
    const printServerAddressInput =
        printServerDialog.shadowRoot!.querySelector<CrInputElement>(
            '#printServerAddressInput');
    assertTrue(!!printServerAddressInput);
    // Assert that the address input field is invalid.
    assertTrue(printServerAddressInput.invalid);
  });

  test('AddPrintServerConnectionError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters = {printerList: []};
    await addPrintServer('serverAddress', PrintServerResult.CONNECTION_ERROR);
    flush();
    verifyErrorMessage('printServerConnectionError');
  });

  test('AddPrintServerReachableServerButIppResponseError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters = {printerList: []};
    await addPrintServer(
        'serverAddress', PrintServerResult.CANNOT_PARSE_IPP_RESPONSE);
    flush();
    verifyErrorMessage('printServerConfigurationErrorMessage');
  });

  test('AddPrintServerReachableServerButHttpResponseError', async () => {
    cupsPrintersBrowserProxy.printServerPrinters = {printerList: []};
    await addPrintServer('serverAddress', PrintServerResult.HTTP_ERROR);
    flush();
    verifyErrorMessage('printServerConfigurationErrorMessage');
  });
});
