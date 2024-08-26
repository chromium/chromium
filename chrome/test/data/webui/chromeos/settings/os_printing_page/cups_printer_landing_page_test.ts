// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {CupsPrinterInfo, CupsPrintersBrowserProxyImpl, CupsPrintersEntryManager, PrinterListEntry, PrinterSettingsUserAction, PrinterStatusReason, PrinterStatusSeverity, PrinterType, SettingsCupsEditPrinterDialogElement, SettingsCupsEnterprisePrintersElement, SettingsCupsNearbyPrintersElement, SettingsCupsPrintersElement, SettingsCupsPrintersEntryElement, SettingsCupsSavedPrintersElement} from 'chrome://os-settings/lazy_load.js';
import {CrInputElement, CrSearchableDropDownElement, CrToastElement, Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {IronIconElement} from 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNotReached, assertNull, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/chromeos/test_util.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {createCupsPrinterInfo, createPrinterListEntry, getPrinterEntries} from './cups_printer_test_utils.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

const arrowUpEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});

const arrowDownEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

const arrowLeftEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});

const arrowRightEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

function clickButton(button: HTMLButtonElement): void {
  assertFalse(button.disabled);
  button.click();
  flush();
}

function initializeEditDialog(page: HTMLElement):
    SettingsCupsEditPrinterDialogElement {
  const editDialog =
      page.shadowRoot!.querySelector<SettingsCupsEditPrinterDialogElement>(
          '#editPrinterDialog');
  assertTrue(!!editDialog);

  // Edit dialog will reset |ppdModel| when |ppdManufacturer| is set. Must
  // set values separately.
  editDialog.set('pendingPrinter_.ppdManufacturer', 'make');
  editDialog.set('pendingPrinter_.ppdModel', 'model');

  // Initializing |activePrinter| in edit dialog will set
  // |needsReconfigured_| to true. Reset it so that any changes afterwards
  // mimics user input.
  editDialog.set('needsReconfigured_', false);

  return editDialog;
}

function verifyErrorToastMessage(
    expectedMessage: string, toast: CrToastElement): void {
  assertTrue(toast.open);
  assertEquals(expectedMessage, toast.textContent?.trim());
}

/**
 * Helper function that verifies that |printerList| matches the printers in
 * |entryList|.
 */
function verifyPrintersList(
    entryList: NodeListOf<SettingsCupsPrintersEntryElement>,
    printerList: CupsPrinterInfo[]): void {
  for (let index = 0; index < printerList.length; ++index) {
    const entryInfo = entryList[index]!.printerEntry.printerInfo;
    const printerInfo = printerList[index];
    assertTrue(!!printerInfo);
    assertEquals(printerInfo.printerName, entryInfo.printerName);
    assertEquals(printerInfo.printerAddress, entryInfo.printerAddress);
    assertEquals(printerInfo.printerId, entryInfo.printerId);
    assertEquals(entryList.length, printerList.length);
  }
}

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 */
function verifyFilteredPrinters(
    printerEntryListTestElement: Element, searchTerm: string): void {
  const printerListEntries: SettingsCupsPrintersEntryElement[] =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry'));
  const hiddenEntries: SettingsCupsPrintersEntryElement[] =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry[hidden]'));

  printerListEntries.forEach(entry => {
    if (hiddenEntries.indexOf(entry) === -1) {
      assertStringContains(
          entry.printerEntry.printerInfo.printerName.toLowerCase(),
          searchTerm.toLowerCase());
    }
  });
}

/**
 * Helper function to verify that the actual visible printers match the
 * expected printer list.
 */
function verifyVisiblePrinters(
    printerEntryListTestElement: Element,
    expectedVisiblePrinters: PrinterListEntry[]): void {
  const actualPrinterList: SettingsCupsPrintersEntryElement[] =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry:not([hidden])'));

  assertEquals(expectedVisiblePrinters.length, actualPrinterList.length);
  for (let i = 0; i < expectedVisiblePrinters.length; i++) {
    const expectedPrinter = expectedVisiblePrinters[i]!.printerInfo;
    const actualPrinter = actualPrinterList[i]!.printerEntry.printerInfo;

    assertEquals(expectedPrinter.printerName, actualPrinter.printerName);
    assertEquals(expectedPrinter.printerAddress, actualPrinter.printerAddress);
    assertEquals(expectedPrinter.printerId, actualPrinter.printerId);
  }
}

/**
 * Helper function to verify that printers are hidden accordingly if they do not
 * match the search query. Also checks if the no search results section is shown
 * when appropriate.
 */
function verifySearchQueryResults(
    printersElement: Element, expectedVisiblePrinters: PrinterListEntry[],
    searchTerm: string): void {
  const printerEntryListTestElement =
      printersElement.shadowRoot!.querySelector('#printerEntryList');
  assertTrue(!!printerEntryListTestElement);
  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  const noSearchResults =
      printersElement.shadowRoot!.querySelector<HTMLElement>(
          '#no-search-results');
  assertTrue(!!noSearchResults);

  if (expectedVisiblePrinters.length) {
    assertTrue(noSearchResults.hidden);
  } else {
    assertFalse(noSearchResults.hidden);
  }
}

/**
 * Removes a saved printer located at |index|.
 */
async function removePrinter(
    cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy,
    savedPrintersElement: HTMLElement, index: number): Promise<void> {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;
  const savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  const button =
      savedPrinterEntries[index]!.shadowRoot!.querySelector<HTMLButtonElement>(
          '.icon-more-vert');
  assertTrue(!!button);
  clickButton(button);
  const removeButton =
      savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
          '#removeButton');
  assertTrue(!!removeButton);
  clickButton(removeButton);

  await cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter');
  // Simulate removing the printer from |cupsPrintersBrowserProxy|.
  printerList.splice(index, 1);

  // Simulate saved printer changes.
  webUIListenerCallback(
      'on-saved-printers-changed', cupsPrintersBrowserProxy.printerList);
  flush();
}

/**
 * Removes all saved printers through recursion.
 */
async function removeAllPrinters(
    cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy,
    savedPrintersElement: HTMLElement): Promise<void> {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;

  if (!printerList.length) {
    return;
  }

  await removePrinter(
      cupsPrintersBrowserProxy, savedPrintersElement, 0 /* index */);
  await flushTasks();
  await removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement);
}

suite('CupsSavedPrintersTests', () => {
  let page: SettingsCupsPrintersElement;
  let savedPrintersElement: SettingsCupsSavedPrintersElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;
  let printerList: CupsPrinterInfo[];

  setup(() => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement.remove();
    CupsPrintersEntryManager.resetForTesting();
  });


  function createCupsPrinterPage(printers: CupsPrinterInfo[]): void {
    printerList = printers;
    // |cupsPrinterBrowserProxy| needs to have a list of saved printers before
    // initializing the landing page.
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);

    flush();
  }

  function addNewSavedPrinter(printer: CupsPrinterInfo): void {
    printerList.push(printer);
    updateSavedPrinters();
  }

  function removeSavedPrinter(id: string): void {
    const idx = printerList.findIndex(p => p.printerId === id);
    printerList.splice(idx, 1);
    updateSavedPrinters();
  }

  function updateSavedPrinters(): void {
    cupsPrintersBrowserProxy.printerList = {printerList};
    webUIListenerCallback(
        'on-saved-printers-changed', cupsPrintersBrowserProxy.printerList);
    flush();
  }

  test('SavedPrintersSuccessfullyPopulates', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    // List component contained by CupsSavedPrinters.
    const printerListEntries = getPrinterEntries(savedPrintersElement);
    verifyPrintersList(printerListEntries, printerList);
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    await removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement);
    const entryList = getPrinterEntries(savedPrintersElement);
    verifyPrintersList(entryList, printerList);
  });

  test('HideSavedPrintersWhenEmpty', async () => {
    // List component contained by CupsSavedPrinters.
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const savedPrinterEntries = getPrinterEntries(savedPrintersElement);
    verifyPrintersList(savedPrinterEntries, printerList);
    assertTrue(!!page.shadowRoot!.querySelector('#savedPrinters'));
    await removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement);
    assertTrue(isVisible(page.shadowRoot!.querySelector('#noSavedPrinters')));
    assertEquals(0, getPrinterEntries(savedPrintersElement).length);
  });

  test('UpdateSavedPrinter', async () => {
    const expectedName = 'edited name';

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const savedPrinterEntries = getPrinterEntries(savedPrintersElement);
    // Update the printer name of the first entry.
    const button =
        savedPrinterEntries[0]!.shadowRoot!.querySelector<HTMLButtonElement>(
            '.icon-more-vert');
    assertTrue(!!button);
    clickButton(button);
    const editButton =
        savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editButton');
    assertTrue(!!editButton);
    clickButton(editButton);
    flush();
    const editDialog = initializeEditDialog(page);
    // Change name of printer and save the change.
    const nameField = editDialog.shadowRoot!.querySelector<CrInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = expectedName;
    nameField.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    flush();
    const actionButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    clickButton(actionButton);
    await cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
    assertEquals(expectedName, editDialog.activePrinter.printerName);
    // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
    printerList[0]!.printerName = expectedName;
    verifyPrintersList(savedPrinterEntries, printerList);
  });

  test('ReconfigureSavedPrinter', async () => {
    const expectedName = 'edited name';
    const expectedAddress = '1.1.1.1';

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const savedPrinterEntries = getPrinterEntries(savedPrintersElement);
    // Edit the first entry.
    const button =
        savedPrinterEntries[0]!.shadowRoot!.querySelector<HTMLButtonElement>(
            '.icon-more-vert');
    assertTrue(!!button);
    clickButton(button);
    const editButton =
        savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editButton');
    assertTrue(!!editButton);
    clickButton(editButton);
    flush();
    const editDialog = initializeEditDialog(page);
    const nameField = editDialog.shadowRoot!.querySelector<CrInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    nameField.value = expectedName;
    nameField.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    const addressField =
        editDialog.shadowRoot!.querySelector<CrInputElement>('#printerAddress');
    assertTrue(!!addressField);
    addressField.value = expectedAddress;
    addressField.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));
    const cancelButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.cancel-button');
    assertTrue(!!cancelButton);
    assertFalse(cancelButton.hidden);
    const actionButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    assertFalse(actionButton.hidden);
    flush();
    clickButton(actionButton);
    await cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
    assertEquals(expectedName, editDialog.activePrinter.printerName);
    assertEquals(expectedAddress, editDialog.activePrinter.printerAddress);
    // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
    printerList[0]!.printerName = expectedName;
    printerList[0]!.printerAddress = expectedAddress;
    verifyPrintersList(savedPrinterEntries, printerList);
  });

  test('SavedPrintersSearchTermFiltersCorrectPrinters', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerListEntries = getPrinterEntries(savedPrintersElement);
    verifyPrintersList(printerListEntries, printerList);
    let searchTerm = 'google';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    // Filtering "google" should result in one visible entry and two
    // hidden entries.
    verifySearchQueryResults(
        savedPrintersElement,
        [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
        searchTerm);
    // Change the search term and assert that entries are filtered
    // correctly. Filtering "test" should result in three visible entries
    // and one hidden entry.
    searchTerm = 'test';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        savedPrintersElement,
        [
          createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
          createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
        ],
        searchTerm);
    // Add more printers and assert that they are correctly filtered.
    addNewSavedPrinter(createCupsPrinterInfo('test3', '3', 'id3'));
    addNewSavedPrinter(createCupsPrinterInfo('google2', '6', 'id6'));
    flush();
    verifySearchQueryResults(
        savedPrintersElement,
        [
          createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
          createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
          createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
        ],
        searchTerm);
  });

  test('SavedPrintersNoSearchFound', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerListEntries = getPrinterEntries(savedPrintersElement);
    verifyPrintersList(printerListEntries, printerList);
    let searchTerm = 'google';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    // Filtering "google" should result in one visible entry and three
    // hidden entries.
    verifySearchQueryResults(
        savedPrintersElement,
        [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
        searchTerm);
    // Change search term to something that has no matches.
    searchTerm = 'noSearchFound';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(savedPrintersElement, [], searchTerm);
    // Change search term back to "google" and verify that the No search
    // found message is no longer there.
    searchTerm = 'google';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        savedPrintersElement,
        [createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED)],
        searchTerm);
  });

  test('NavigateSavedPrintersList', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryList =
        savedPrintersElement.shadowRoot!.querySelector<HTMLElement>(
            '#printerEntryList');
    assertTrue(!!printerEntryList);
    const printerListEntries = getPrinterEntries(savedPrintersElement);
    printerEntryList.focus();
    printerEntryList.dispatchEvent(arrowDownEvent);
    flush();
    assertEquals(printerListEntries[1], getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowDownEvent);
    flush();
    assertEquals(printerListEntries[2], getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowUpEvent);
    flush();
    assertEquals(printerListEntries[1], getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowUpEvent);
    flush();
    assertEquals(printerListEntries[0], getDeepActiveElement());
  });

  test('Deep link to saved printers', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);

    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');

    const params = new URLSearchParams();
    params.append('settingId', settingMojom.Setting.kSavedPrinters.toString());
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS, params);

    flush();

    const savedPrinters =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    const printerEntry = savedPrinters &&
        savedPrinters.shadowRoot!.querySelector('settings-cups-printers-entry');
    const deepLinkElement = printerEntry &&
        printerEntry.shadowRoot!.querySelector<HTMLElement>('#moreActions');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    const activeDeepLink = getDeepActiveElement();
    assertEquals(
        deepLinkElement, activeDeepLink,
        'First saved printer menu button should be focused for settingId=1401.');
  });

  test('SavedPrintersStatusUpdates', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);

    cupsPrintersBrowserProxy.addPrinterStatus(
        'id1', PrinterStatusReason.NO_ERROR, PrinterStatusSeverity.ERROR);
    cupsPrintersBrowserProxy.addPrinterStatus(
        'id2', PrinterStatusReason.PRINTER_UNREACHABLE,
        PrinterStatusSeverity.ERROR);

    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    await flushTasks();

    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;

    // The printer statuses should be added to the cache once fetched.
    const printerStatusReasonCache =
        savedPrintersElement.getPrinterStatusReasonCacheForTesting();
    assertTrue(printerStatusReasonCache.has('id1'));
    assertTrue(printerStatusReasonCache.has('id2'));
    assertFalse(printerStatusReasonCache.has('id3'));

    // For each of the 3 saved printers verify it gets the correct printer
    // icon based on the printer status previously set.
    const printerListEntries = getPrinterEntries(savedPrintersElement);
    assertEquals(3, printerListEntries.length);
    for (const entry of printerListEntries) {
      let expectedPrinterIcon;
      switch (entry.printerEntry.printerInfo.printerId) {
        case 'id1':
          expectedPrinterIcon = 'os-settings:printer-status-illo-green';
          break;
        case 'id2':
          expectedPrinterIcon = 'os-settings:printer-status-illo-red';
          break;
        case 'id3':
          expectedPrinterIcon = 'os-settings:printer-status-illo-grey';
          break;
        default:
          assertNotReached();
      }
      const printerStatusIcon =
          entry.shadowRoot!.querySelector<IronIconElement>(
              '#printerStatusIcon');
      assertTrue(!!printerStatusIcon);
      assertEquals(expectedPrinterIcon, printerStatusIcon.icon);
    }

    // Removing the printers should also remove their cache entry.
    await removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement);
    assertFalse(printerStatusReasonCache.has('id1'));
    assertFalse(printerStatusReasonCache.has('id2'));
  });

  // Verify the printer statuses received from the 'local-printers-updated'
  // event are added to the printer status cache.
  test('LocalPrintersUpdatedPrinterStatusCache', async () => {
    createCupsPrinterPage([]);
    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;

    const id1 = 'id1';
    const printer1 = createCupsPrinterInfo('test1', '1', id1);
    printer1.printerStatus = {
      printerId: id1,
      statusReasons: [
        {
          reason: PrinterStatusReason.PRINTER_UNREACHABLE,
          severity: PrinterStatusSeverity.ERROR,
        },
      ],
      timestamp: 0,
    };
    const id2 = 'id2';
    const printer2 = createCupsPrinterInfo('test2', '2', id2);
    printer2.printerStatus = {
      printerId: id2,
      statusReasons: [
        {
          reason: PrinterStatusReason.LOW_ON_INK,
          severity: PrinterStatusSeverity.ERROR,
        },
      ],
      timestamp: 0,
    };
    // Printer3 has an undefined printer status so it shouldn't be added to the
    // cache.
    const id3 = 'id3';
    const printer3 = createCupsPrinterInfo('test3', '3', id3);
    printer3.printerStatus = {
      printerId: '',
      statusReasons: [],
      timestamp: 0,
    };

    // The printer status cache should initialize empty.
    const printerStatusReasonCache =
        savedPrintersElement.getPrinterStatusReasonCacheForTesting();
    assertFalse(printerStatusReasonCache.has(id1));
    assertFalse(printerStatusReasonCache.has(id2));
    assertFalse(printerStatusReasonCache.has(id3));

    // Trigger the observer and expect the printer statuses to be extracted then
    // added to the cache.
    webUIListenerCallback('local-printers-updated', [printer1, printer2]);
    await flushTasks();
    assertTrue(printerStatusReasonCache.has(id1));
    assertTrue(printerStatusReasonCache.has(id2));
    assertFalse(printerStatusReasonCache.has(id3));
  });

  test('ShowMoreButtonIsInitiallyHiddenAndANewPrinterIsAdded', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is hidden because printer list
    // length is <= 3.
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
    // Newly added printers will always be visible and inserted to the
    // top of the list.
    addNewSavedPrinter(createCupsPrinterInfo('test3', '3', 'id3'));
    const expectedVisiblePrinters = [
      createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ];
    verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
    // Assert that the Show more button is still hidden because all newly
    // added printers are visible.
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
  });

  test('PressShowMoreButton', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 4 total printers but only 3 printers are visible and 1 is
    // hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    const showMoreIcon =
        savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#show-more-icon');
    assertTrue(!!showMoreIcon);
    // Click on the Show more button.
    clickButton(showMoreIcon);
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
    // Clicking on the Show more button reveals all hidden printers.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
      createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
    ]);
  });

  test('ShowMoreButtonIsInitiallyShownAndWithANewPrinterAdded', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 4 total printers but only 3 printers are visible and 1 is
    // hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Newly added printers will always be visible.
    addNewSavedPrinter(createCupsPrinterInfo('test5', '5', 'id5'));
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('test5', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is still shown.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));

    // Verify all printers are visible after the Show more button is pressed.
    const showMoreIcon =
        savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#show-more-icon');
    assertTrue(!!showMoreIcon);
    clickButton(showMoreIcon);
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('test5', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
      createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
    ]);
  });

  test('ShowMoreButtonIsShownAndRemovePrinters', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 5 total printers but only 3 printers are visible and 2
    // are hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Simulate removing 'google' printer.
    removeSavedPrinter('id3');
    // Printer list has 4 elements now, but since the list is still
    // collapsed we should still expect only 3 elements to be visible.
    // Since printers were initially alphabetically sorted, we should
    // expect 'test1' to be the next visible printer.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
    ]);
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Simulate removing 'google2' printer.
    removeSavedPrinter('id4');
    // Printer list has 3 elements now, the Show more button should be
    // hidden.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
  });

  test('ShowMoreButtonIsShownAndSearchQueryFiltersCorrectly', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('google4', '6', 'id6'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 6 total printers but only 3 printers are visible and 3
    // are hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Set search term to 'google' and expect 4 visible printers.
    let searchTerm = 'google';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        savedPrintersElement,
        [
          createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
          createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
          createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
          createPrinterListEntry('google4', '6', 'id6', PrinterType.SAVED),
        ],
        searchTerm);
    // Having a search term should hide the Show more button.
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
    // Search for a term with no matching printers. Expect Show more
    // button to still be hidden.
    searchTerm = 'noSearchFound';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(savedPrintersElement, [], searchTerm);
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
    // Change search term and expect new set of visible printers.
    searchTerm = 'test';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        savedPrintersElement,
        [
          createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
          createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
        ],
        searchTerm);
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
    // Remove the search term and expect the collapsed list to appear
    // again.
    searchTerm = '';
    savedPrintersElement.searchTerm = searchTerm;
    flush();
    const expectedVisiblePrinters = [
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
    ];
    verifySearchQueryResults(
        savedPrintersElement, expectedVisiblePrinters, searchTerm);
    verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
  });

  test('ShowMoreButtonAddAndRemovePrinters', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    const printerEntryListTestElement =
        savedPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 4 total printers but only 3 printers are visible and 1 is
    // hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Add a new printer and expect it to be at the top of the list.
    addNewSavedPrinter(createCupsPrinterInfo('newPrinter', '5', 'id5'));
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
      createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
    ]);
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Now simulate removing printer 'test1'.
    removeSavedPrinter('id1');
    // If the number of visible printers is > 3, removing printers will
    // decrease the number of visible printers until there are only 3
    // visible printers. In this case, we remove 'test1' and now only
    // have 3 visible printers and 1 hidden printer: 'test2'.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
    ]);
    assertTrue(!!savedPrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Remove another printer and assert that we still have 3 visible
    // printers but now 'test2' is our third visible printer.
    removeSavedPrinter('id4');
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
      createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
    ]);
    // Printer list length is <= 3, Show more button should be hidden.
    assertNull(
        savedPrintersElement.shadowRoot!.querySelector('#show-more-container'));
  });

  test('RecordUserActionMetric', async () => {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-saved-printers');
    assertTrue(!!element);
    savedPrintersElement = element;
    await removePrinter(
        cupsPrintersBrowserProxy, savedPrintersElement, /*index=*/ 0);
    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.REMOVE_PRINTER));
    // Click the next printer's Edit button then verify the action is
    // recorded.
    const savedPrinterEntries = getPrinterEntries(savedPrintersElement);
    const button =
        savedPrinterEntries[0]!.shadowRoot!.querySelector<HTMLButtonElement>(
            '.icon-more-vert');
    assertTrue(!!button);
    clickButton(button);
    const editButton =
        savedPrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#editButton');
    assertTrue(!!editButton);
    clickButton(editButton);
    assertEquals(
        1,
        fakeMetricsPrivate.countMetricValue(
            'Printing.CUPS.SettingsUserAction',
            PrinterSettingsUserAction.EDIT_PRINTER));
  });
});

suite('CupsNearbyPrintersTests', () => {
  let page: SettingsCupsPrintersElement;
  let nearbyPrintersElement: SettingsCupsNearbyPrintersElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;
  let printerEntryListTestElement: HTMLElement;
  let wifi1: NetworkStateProperties;


  setup(() => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = ConnectionStateType.kOnline;

    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);

    flush();
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    nearbyPrintersElement.remove();
  });

  test('nearbyPrintersSuccessfullyPopulates', async () => {
    const automaticPrinterList = [
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      createCupsPrinterInfo('test3', '3', 'id3'),
      createCupsPrinterInfo('test4', '4', 'id4'),
    ];

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    // Assert that no printers have been detected.
    let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    assertEquals(0, nearbyPrinterEntries.length);
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', automaticPrinterList,
        discoveredPrinterList);
    flush();
    nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    const expectedPrinterList: CupsPrinterInfo[] =
        automaticPrinterList.concat(discoveredPrinterList);
    verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
  });

  test('nearbyPrintersSortOrderAutoFirstThenDiscovered', async () => {
    const discoveredPrinterA =
        createCupsPrinterInfo('printerNameA', 'printerAddress1', 'printerId1');
    const discoveredPrinterB =
        createCupsPrinterInfo('printerNameB', 'printerAddress2', 'printerId2');
    const discoveredPrinterC =
        createCupsPrinterInfo('printerNameC', 'printerAddress3', 'printerId3');
    const autoPrinterD =
        createCupsPrinterInfo('printerNameD', 'printerAddress4', 'printerId4');
    const autoPrinterE =
        createCupsPrinterInfo('printerNameE', 'printerAddress5', 'printerId5');
    const autoPrinterF =
        createCupsPrinterInfo('printerNameF', 'printerAddress6', 'printerId6');

    // Add printers in a non-alphabetical order to test sorting.
    const automaticPrinterList = [autoPrinterF, autoPrinterD, autoPrinterE];
    const discoveredPrinterList =
        [discoveredPrinterC, discoveredPrinterA, discoveredPrinterB];

    // Expected sort order is to sort automatic printers first then
    // sort discovered printers
    const expectedPrinterList = [
      autoPrinterD,
      autoPrinterE,
      autoPrinterF,
      discoveredPrinterA,
      discoveredPrinterB,
      discoveredPrinterC,
    ];

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', automaticPrinterList,
        discoveredPrinterList);
    flush();
    const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
  });

  test('addingAutomaticPrinterIsSuccessful', async () => {
    const automaticPrinterList = [createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList: CupsPrinterInfo[] = [];

    let addButton = null;

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', automaticPrinterList,
        discoveredPrinterList);
    flush();
    // Requery and assert that the newly detected printer automatic
    // printer has the correct button.
    const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    assertEquals(1, nearbyPrinterEntries.length);
    assertTrue(!!nearbyPrinterEntries[0]!.shadowRoot!.querySelector(
        '.save-printer-button'));
    // Add an automatic printer and assert that that the toast
    // notification is shown.
    addButton =
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector<HTMLButtonElement>(
            '.save-printer-button');
    assertTrue(!!addButton);
    clickButton(addButton);
    // Add button should be disabled during setup.
    assertTrue(addButton.disabled);
    await cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
    assertFalse(addButton.disabled);
    const expectedToastMessage =
        'Added ' + automaticPrinterList[0]!.printerName;
    const errorToast =
        page.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    verifyErrorToastMessage(expectedToastMessage, errorToast);
  });

  test('NavigateNearbyPrinterList', async () => {
    const discoveredPrinterList = [
      createCupsPrinterInfo('first', '3', 'id3'),
      createCupsPrinterInfo('second', '4', 'id4'),
      createCupsPrinterInfo('third', '2', 'id5'),
    ];
    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    // Block so that FocusRowBehavior.attached can run.
    await waitAfterNextRender(element);
    nearbyPrintersElement = element;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', [], discoveredPrinterList);
    flush();
    // Wait one more time to ensure that async setup in FocusRowBehavior has
    // executed.
    await waitAfterNextRender(nearbyPrintersElement);
    const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    const printerEntryList =
        nearbyPrintersElement.shadowRoot!.querySelector('#printerEntryList');
    assertTrue(!!printerEntryList);
    const entry =
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector<HTMLElement>(
            '#entry');
    assertTrue(!!entry);
    entry.focus();
    assertEquals(
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
    // Ensure that we can navigate through items in a row
    getDeepActiveElement()?.dispatchEvent(arrowRightEvent);
    assertEquals(
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector(
            '#setupPrinterButton'),
        getDeepActiveElement());
    getDeepActiveElement()?.dispatchEvent(arrowLeftEvent);
    assertEquals(
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
    // Ensure that we can navigate through printer rows
    printerEntryList.dispatchEvent(arrowDownEvent);
    assertEquals(
        nearbyPrinterEntries[1]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowDownEvent);
    assertEquals(
        nearbyPrinterEntries[2]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowUpEvent);
    assertEquals(
        nearbyPrinterEntries[1]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
    printerEntryList.dispatchEvent(arrowUpEvent);
    assertEquals(
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector('#entry'),
        getDeepActiveElement());
  });

  test('addingDiscoveredPrinterIsSuccessful', async () => {
    const automaticPrinterList: CupsPrinterInfo[] = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;
    let setupButton = null;

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', automaticPrinterList,
        discoveredPrinterList);
    flush();
    // Requery and assert that a newly detected discovered printer has
    // the correct icon button.
    const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    assertEquals(1, nearbyPrinterEntries.length);
    assertTrue(!!nearbyPrinterEntries[0]!.shadowRoot!.querySelector(
        '#setupPrinterButton'));
    // Force a failure with adding a discovered printer.
    cupsPrintersBrowserProxy.setAddDiscoveredPrinterFailure(
        discoveredPrinterList[0]!);
    // Assert that clicking on the setup button shows the advanced
    // configuration dialog.
    setupButton =
        nearbyPrinterEntries[0]!.shadowRoot!.querySelector<HTMLButtonElement>(
            '#setupPrinterButton');
    assertTrue(!!setupButton);
    clickButton(setupButton);
    // Setup button should be disabled during setup.
    assertTrue(setupButton.disabled);
    await cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
    assertFalse(setupButton.disabled);
    flush();
    const addDialog = page.shadowRoot!.querySelector('#addPrinterDialog');
    assertTrue(!!addDialog);
    manufacturerDialog = addDialog.shadowRoot!.querySelector(
        'add-printer-manufacturer-model-dialog');
    assertTrue(!!manufacturerDialog);
    await cupsPrintersBrowserProxy.whenCalled(
        'getCupsPrinterManufacturersList');
    const addButton =
        manufacturerDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addPrinterButton');
    assertTrue(!!addButton);
    assertTrue(addButton.disabled);
    // Populate the manufacturer and model fields to enable the Add
    // button.
    const manufacturerDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>(
                '#manufacturerDropdown');
    assertTrue(!!manufacturerDropdown);
    manufacturerDropdown.value = 'make';
    const modelDropdown =
        manufacturerDialog.shadowRoot!
            .querySelector<CrSearchableDropDownElement>('#modelDropdown');
    assertTrue(!!modelDropdown);
    modelDropdown.value = 'model';
    clickButton(addButton);
    await cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
    // Assert that the toast notification is shown and has the expected
    // message when adding a discovered printer.
    const expectedToastMessage =
        'Added ' + discoveredPrinterList[0]!.printerName;
    const errorToast =
        page.shadowRoot!.querySelector<CrToastElement>('#errorToast');
    assertTrue(!!errorToast);
    verifyErrorToastMessage(expectedToastMessage, errorToast);
  });

  test('NetworkConnectedButNoInternet', async () => {
    // Simulate connecting to a network with no internet connection.
    wifi1.connectionState = ConnectionStateType.kConnected;
    page.onActiveNetworksChanged([wifi1]);
    flush();

    await flushTasks();
    // We require internet to be able to add a new printer. Connecting to
    // a network without connectivity should be equivalent to not being
    // connected to a network.
    assertTrue(!!page.shadowRoot!.querySelector('#cloudOffIcon'));
    assertTrue(!!page.shadowRoot!.querySelector('#connectionMessage'));
    const addManualPrinterButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addManualPrinterButton');
    assertTrue(!!addManualPrinterButton);
    assertTrue(addManualPrinterButton.disabled);
  });

  test('checkNetworkConnection', async () => {
    // Simulate disconnecting from a network.
    wifi1.connectionState = ConnectionStateType.kNotConnected;
    page.onActiveNetworksChanged([wifi1]);
    flush();

    await flushTasks();
    // Expect offline text to show up when no internet is
    // connected.
    assertTrue(!!page.shadowRoot!.querySelector('#cloudOffIcon'));
    assertTrue(!!page.shadowRoot!.querySelector('#connectionMessage'));
    const addManualPrinterButton =
        page.shadowRoot!.querySelector<HTMLButtonElement>(
            '#addManualPrinterButton');
    assertTrue(!!addManualPrinterButton);
    assertTrue(addManualPrinterButton.disabled);
    // Simulate connecting to a network with connectivity.
    wifi1.connectionState = ConnectionStateType.kOnline;
    page.onActiveNetworksChanged([wifi1]);
    flush();
    await flushTasks();
    const automaticPrinterList = [
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      createCupsPrinterInfo('test3', '3', 'id3'),
      createCupsPrinterInfo('test4', '4', 'id4'),
    ];
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', automaticPrinterList,
        discoveredPrinterList);
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    const listElement =
        nearbyPrintersElement.shadowRoot!.querySelector<HTMLElement>(
            '#printerEntryList');
    assertTrue(!!listElement);
    printerEntryListTestElement = listElement;
    const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
    const expectedPrinterList =
        automaticPrinterList.concat(discoveredPrinterList);
    verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
  });

  test('NearbyPrintersSearchTermFiltersCorrectPrinters', async () => {
    const discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('test2', 'printerAddress2', 'printerId2'),
      createCupsPrinterInfo('google', 'printerAddress3', 'printerId3'),
    ];

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    const listElement =
        nearbyPrintersElement.shadowRoot!.querySelector<HTMLElement>(
            '#printerEntryList');
    assertTrue(!!listElement);
    printerEntryListTestElement = listElement;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', [], discoveredPrinterList);
    flush();
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry(
          'google', 'printerAddress3', 'printerId3', PrinterType.DISCOVERED),
      createPrinterListEntry(
          'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERED),
      createPrinterListEntry(
          'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERED),
    ]);
    let searchTerm = 'google';
    nearbyPrintersElement.searchTerm = searchTerm;
    flush();
    // Filtering "google" should result in one visible entry and two hidden
    // entries.
    verifySearchQueryResults(
        nearbyPrintersElement,
        [createPrinterListEntry(
            'google', 'printerAddress3', 'printerId3', PrinterType.DISCOVERED)],
        searchTerm);
    // Filtering "test" should result in two visible entries and one hidden
    // entry.
    searchTerm = 'test';
    nearbyPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        nearbyPrintersElement,
        [
          createPrinterListEntry(
              'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERED),
          createPrinterListEntry(
              'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERED),
        ],
        searchTerm);
    // Add more printers and assert that they are correctly filtered.
    discoveredPrinterList.push(
        createCupsPrinterInfo('test3', 'printerAddress4', 'printerId4'));
    discoveredPrinterList.push(
        createCupsPrinterInfo('google2', 'printerAddress5', 'printerId5'));
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', [], discoveredPrinterList);
    flush();
    verifySearchQueryResults(
        nearbyPrintersElement,
        [
          createPrinterListEntry(
              'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERED),
          createPrinterListEntry(
              'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERED),
          createPrinterListEntry(
              'test3', 'printerAddress4', 'printerId4', PrinterType.DISCOVERED),
        ],
        searchTerm);
  });

  test('NearbyPrintersNoSearchFound', async () => {
    const discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('google', 'printerAddress2', 'printerId2'),
    ];

    await flushTasks();
    const element =
        page.shadowRoot!.querySelector('settings-cups-nearby-printers');
    assertTrue(!!element);
    nearbyPrintersElement = element;
    const listElement =
        nearbyPrintersElement.shadowRoot!.querySelector<HTMLElement>(
            '#printerEntryList');
    assertTrue(!!listElement);
    printerEntryListTestElement = listElement;
    // Simuluate finding nearby printers.
    webUIListenerCallback(
        'on-nearby-printers-changed', [], discoveredPrinterList);
    flush();
    let searchTerm = 'google';
    nearbyPrintersElement.searchTerm = searchTerm;
    flush();
    // Set the search term and filter out the printers. Filtering "google"
    // should result in one visible entry and one hidden entries.
    verifySearchQueryResults(
        nearbyPrintersElement,
        [createPrinterListEntry(
            'google', 'printerAddress2', 'printerId2', PrinterType.DISCOVERED)],
        searchTerm);
    // Change search term to something that has no matches.
    searchTerm = 'noSearchFound';
    nearbyPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(nearbyPrintersElement, [], searchTerm);
    // Change search term back to "google" and verify that the No search found
    // message is no longer there.
    searchTerm = 'google';
    nearbyPrintersElement.searchTerm = searchTerm;
    flush();
    verifySearchQueryResults(
        nearbyPrintersElement,
        [createPrinterListEntry(
            'google', 'printerAddress2', 'printerId2', PrinterType.DISCOVERED)],
        searchTerm);
  });
});

suite('CupsEnterprisePrintersTests', () => {
  let page: SettingsCupsPrintersElement;
  let enterprisePrintersElement: SettingsCupsEnterprisePrintersElement;
  let printerEntryListTestElement: HTMLElement;
  let cupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy;
  let printerList: CupsPrinterInfo[];

  setup(() => {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  });

  teardown(() => {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    enterprisePrintersElement.remove();
  });


  function createCupsPrinterPage(printers: CupsPrinterInfo[]) {
    printerList = printers;
    // |cupsPrinterBrowserProxy| needs to have a list of printers before
    // initializing the landing page.
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);

    flush();
  }

  // Verifies that enterprise printers are correctly shown on the OS settings
  // page.
  test('EnterprisePrinters', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', /*isManaged=*/ true),
      createCupsPrinterInfo('test2', '2', 'id2', /*isManaged=*/ true),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList');
    // Wait for saved printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-enterprise-printers');
    assertTrue(!!element);
    enterprisePrintersElement = element;
    const listElement =
        enterprisePrintersElement.shadowRoot!.querySelector<HTMLElement>(
            '#printerEntryList');
    assertTrue(!!listElement);
    printerEntryListTestElement = listElement;
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('test1', '1', 'id1', PrinterType.ENTERPRISE),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.ENTERPRISE),
    ]);
  });

  // Verifies that enterprise printers are not editable.
  test('EnterprisePrinterDialog', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', true),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList');
    // Wait for enterprise printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-enterprise-printers');
    assertTrue(!!element);
    enterprisePrintersElement = element;
    const enterprisePrinterEntries:
        NodeListOf<SettingsCupsPrintersEntryElement> =
            getPrinterEntries(enterprisePrintersElement);
    // Users are not allowed to remove enterprise printers.
    const removeButton =
        enterprisePrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#removeButton');
    assertTrue(!!removeButton);
    assertTrue(removeButton.disabled);
    const button = enterprisePrinterEntries[0]!.shadowRoot!
                       .querySelector<HTMLButtonElement>('.icon-more-vert');
    assertTrue(!!button);
    clickButton(button);
    const viewButton =
        enterprisePrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#viewButton');
    assertTrue(!!viewButton);
    clickButton(viewButton);
    flush();
    const editDialog: SettingsCupsEditPrinterDialogElement =
        initializeEditDialog(page);
    const nameField = editDialog.shadowRoot!.querySelector<CrInputElement>(
        '.printer-name-input');
    assertTrue(!!nameField);
    assertEquals('test1', nameField.value);
    assertTrue(nameField.readonly);
    const printerAddress =
        editDialog.shadowRoot!.querySelector<CrInputElement>('#printerAddress');
    assertTrue(!!printerAddress);
    assertTrue(printerAddress.readonly);
    const selectElement =
        editDialog.shadowRoot!.querySelector<HTMLSelectElement>('.md-select');
    assertTrue(!!selectElement);
    assertTrue(selectElement.disabled);
    const printerQueue =
        editDialog.shadowRoot!.querySelector<CrInputElement>('#printerQueue');
    assertTrue(!!printerQueue);
    assertTrue(printerQueue.readonly);
    const printerPPDManufacturer =
        editDialog.shadowRoot!.querySelector<CrSearchableDropDownElement>(
            '#printerPPDManufacturer');
    assertTrue(!!printerPPDManufacturer);
    assertTrue(printerPPDManufacturer.readonly);
    // The "specify PDD" section should be hidden.
    const browseButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.browse-button');
    assertTrue(!!browseButton);
    const parentElement = browseButton.parentElement;
    assertTrue(!!parentElement);
    assertTrue(parentElement.hidden);
    const ppdLabel =
        editDialog.shadowRoot!.querySelector<HTMLElement>('#ppdLabel');
    assertTrue(!!ppdLabel);
    assertTrue(ppdLabel.hidden);
    // Save and Cancel buttons should be hidden. Close button should be
    // visible.
    const cancelButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.cancel-button');
    assertTrue(!!cancelButton);
    assertTrue(cancelButton.hidden);
    const actionButton =
        editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
            '.action-button');
    assertTrue(!!actionButton);
    assertTrue(actionButton.hidden);
    const closeButton = editDialog.shadowRoot!.querySelector<HTMLButtonElement>(
        '.close-button');
    assertTrue(!!closeButton);
    assertFalse(closeButton.hidden);
  });

  test('PressShowMoreButton', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', true),
      createCupsPrinterInfo('test2', '2', 'id2', true),
      createCupsPrinterInfo('test3', '3', 'id3', true),
      createCupsPrinterInfo('test4', '4', 'id4', true),
    ]);
    await cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList');
    // Wait for enterprise printers to populate.
    flush();
    const element =
        page.shadowRoot!.querySelector('settings-cups-enterprise-printers');
    assertTrue(!!element);
    enterprisePrintersElement = element;
    const printerEntryListTestElement =
        enterprisePrintersElement.shadowRoot!.querySelector(
            '#printerEntryList');
    assertTrue(!!printerEntryListTestElement);
    // There are 4 total printers but only 3 printers are visible and 1 is
    // hidden underneath the Show more section.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('test1', '1', 'id1', PrinterType.ENTERPRISE),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.ENTERPRISE),
      createPrinterListEntry('test3', '3', 'id3', PrinterType.ENTERPRISE),
    ]);
    // Assert that the Show more button is shown since printer list length
    // is > 3.
    assertTrue(!!enterprisePrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));

    const showMoreIcon =
        enterprisePrintersElement.shadowRoot!.querySelector<HTMLButtonElement>(
            '#show-more-icon');
    assertTrue(!!showMoreIcon);
    // Click on the Show more button.
    clickButton(showMoreIcon);
    assertNull(enterprisePrintersElement.shadowRoot!.querySelector(
        '#show-more-container'));
    // Clicking on the Show more button reveals all hidden printers.
    verifyVisiblePrinters(printerEntryListTestElement, [
      createPrinterListEntry('test1', '1', 'id1', PrinterType.ENTERPRISE),
      createPrinterListEntry('test2', '2', 'id2', PrinterType.ENTERPRISE),
      createPrinterListEntry('test3', '3', 'id3', PrinterType.ENTERPRISE),
      createPrinterListEntry('test4', '4', 'id4', PrinterType.ENTERPRISE),
    ]);
  });
});
