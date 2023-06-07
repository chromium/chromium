// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CupsPrintersBrowserProxyImpl, PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS, PrinterSettingsUserAction, PrinterStatusReason, PrinterStatusSeverity, PrinterType} from 'chrome://os-settings/lazy_load.js';
import {Router, routes} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/ash/common/cr.m.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {NetworkStateProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {ConnectionStateType, NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertGE, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createCupsPrinterInfo, createPrinterListEntry, getPrinterEntries} from './cups_printer_test_utils.js';
import {FakeMetricsPrivate} from './fake_metrics_private.js';
import {TestCupsPrintersBrowserProxy} from './test_cups_printers_browser_proxy.js';

const arrowUpEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});

const arrowDownEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

const arrowLeftEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});

const arrowRightEvent = new KeyboardEvent(
    'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

/**
 * @param {!HTMLElement} button
 * @private
 */
function clickButton(button) {
  assertTrue(!!button);
  assertTrue(!button.disabled);

  button.click();
  flush();
}

/**
 * @param {!HTMLElement} page
 * @return {!HTMLElement}
 * @private
 */
function initializeEditDialog(page) {
  const editDialog = page.shadowRoot.querySelector('#editPrinterDialog');
  assertTrue(!!editDialog);

  // Edit dialog will reset |ppdModel| when |ppdManufacturer| is set. Must
  // set values separately.
  editDialog.pendingPrinter_.ppdManufacturer = 'make';
  editDialog.pendingPrinter_.ppdModel = 'model';

  // Initializing |activePrinter| in edit dialog will set
  // |needsReconfigured_| to true. Reset it so that any changes afterwards
  // mimics user input.
  editDialog.needsReconfigured_ = false;

  return editDialog;
}

/**
 * @param {string} expectedMessage
 * @param {!HTMLElement} toast
 * @private
 */
function verifyErrorToastMessage(expectedMessage, toast) {
  assertTrue(toast.open);
  assertEquals(expectedMessage, toast.textContent.trim());
}

/**
 * Helper function that verifies that |printerList| matches the printers in
 * |entryList|.
 * @param {!HTMLElement} entryList
 * @private
 */
function verifyPrintersList(entryList, printerList) {
  for (let index = 0; index < printerList.length; ++index) {
    const entryInfo = entryList[index].printerEntry.printerInfo;
    const printerInfo = printerList[index];

    assertEquals(printerInfo.printerName, entryInfo.printerName);
    assertEquals(printerInfo.printerAddress, entryInfo.printerAddress);
    assertEquals(printerInfo.printerId, entryInfo.printerId);
    assertEquals(entryList.length, printerList.length);
  }
}

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 * @param {!Element} printerEntryListTestElement
 * @param {string} searchTerm
 */
function verifyFilteredPrinters(printerEntryListTestElement, searchTerm) {
  const printerListEntries =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry'));
  const hiddenEntries = Array.from(printerEntryListTestElement.querySelectorAll(
      'settings-cups-printers-entry[hidden]'));

  for (let i = 0; i < printerListEntries.length; ++i) {
    const entry = printerListEntries[i];
    if (hiddenEntries.indexOf(entry) === -1) {
      assertTrue(
          entry.printerEntry.printerInfo.printerName.toLowerCase().includes(
              searchTerm.toLowerCase()));
    }
  }
}

/**
 * Helper function to verify that the actual visible printers match the
 * expected printer list.
 * @param {!Element} printerEntryListTestElement

 */
function verifyVisiblePrinters(
    printerEntryListTestElement, expectedVisiblePrinters) {
  const actualPrinterList =
      Array.from(printerEntryListTestElement.querySelectorAll(
          'settings-cups-printers-entry:not([hidden])'));

  assertEquals(expectedVisiblePrinters.length, actualPrinterList.length);
  for (let i = 0; i < expectedVisiblePrinters.length; i++) {
    const expectedPrinter = expectedVisiblePrinters[i].printerInfo;
    const actualPrinter = actualPrinterList[i].printerEntry.printerInfo;

    assertEquals(actualPrinter.printerName, expectedPrinter.printerName);
    assertEquals(actualPrinter.printerAddress, expectedPrinter.printerAddress);
    assertEquals(actualPrinter.printerId, expectedPrinter.printerId);
  }
}

/**
 * Helper function to verify that printers are hidden accordingly if they do not
 * match the search query. Also checks if the no search results section is shown
 * when appropriate.
 * @param {!Element} printersElement

 * @param {string} searchTerm
 */
function verifySearchQueryResults(
    printersElement, expectedVisiblePrinters, searchTerm) {
  const printerEntryListTestElement =
      printersElement.shadowRoot.querySelector('#printerEntryList');

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(
        printersElement.shadowRoot.querySelector('#no-search-results').hidden);
  } else {
    assertFalse(
        printersElement.shadowRoot.querySelector('#no-search-results').hidden);
  }
}

/**
 * Removes a saved printer located at |index|.

 * @param {!HTMLElement} savedPrintersElement
 * @param {number} index
 * @return {!Promise}
 */
function removePrinter(cupsPrintersBrowserProxy, savedPrintersElement, index) {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;
  const savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  clickButton(
      savedPrinterEntries[index].shadowRoot.querySelector('.icon-more-vert'));
  clickButton(savedPrintersElement.shadowRoot.querySelector('#removeButton'));

  return cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter')
      .then(function() {
        // Simulate removing the printer from |cupsPrintersBrowserProxy|.
        printerList.splice(index, 1);

        // Simuluate saved printer changes.
        webUIListenerCallback(
            'on-saved-printers-changed', cupsPrintersBrowserProxy.printerList);
        flush();
      });
}

/**
 * Removes all saved printers through recursion.

 * @param {!HTMLElement} savedPrintersElement
 * @return {!Promise}
 */
function removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement) {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;
  const savedPrinterEntries = getPrinterEntries(savedPrintersElement);

  if (!printerList.length) {
    return Promise.resolve();
  }

  return removePrinter(
             cupsPrintersBrowserProxy, savedPrintersElement, 0 /* index */)
      .then(flushTasks)
      .then(removeAllPrinters.bind(
          this, cupsPrintersBrowserProxy, savedPrintersElement));
}

suite('CupsSavedPrintersTests', function() {
  let page = null;
  let savedPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type {?Array<!CupsPrinterInfo>} */
  let printerList = null;

  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement = null;
    printerList = null;
    page = null;
  });


  function createCupsPrinterPage(printers) {
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


  function addNewSavedPrinter(printer) {
    printerList.push(printer);
    updateSavedPrinters();
  }

  /** @param {number} id*/
  function removeSavedPrinter(id) {
    const idx = printerList.findIndex(p => p.printerId === id);
    printerList.splice(idx, 1);
    updateSavedPrinters();
  }

  function updateSavedPrinters() {
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    webUIListenerCallback(
        'on-saved-printers-changed', cupsPrintersBrowserProxy.printerList);
    flush();
  }

  test('SavedPrintersSuccessfullyPopulates', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          // List component contained by CupsSavedPrinters.
          const savedPrintersList =
              savedPrintersElement.shadowRoot.querySelector(
                  'settings-cups-printers-entry-list');

          const printerListEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(printerListEntries, printerList);
        });
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', function() {
    const savedPrinterEntries = [];

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          const entryList = getPrinterEntries(savedPrintersElement);
          verifyPrintersList(entryList, printerList);
        });
  });

  test('HideSavedPrintersWhenEmpty', function() {
    // List component contained by CupsSavedPrinters.
    let savedPrintersList = [];
    let savedPrinterEntries = [];

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrintersList = savedPrintersElement.shadowRoot.querySelector(
              'settings-cups-printers-entry-list');
          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          verifyPrintersList(savedPrinterEntries, printerList);

          assertTrue(!!page.shadowRoot.querySelector('#savedPrinters'));

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          assertFalse(!!page.shadowRoot.querySelector('#savedPrinters'));
        });
  });

  test('UpdateSavedPrinter', function() {
    const expectedName = 'edited name';

    let editDialog = null;
    let savedPrinterEntries = null;

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Update the printer name of the first entry.
          clickButton(savedPrinterEntries[0].shadowRoot.querySelector(
              '.icon-more-vert'));
          clickButton(
              savedPrintersElement.shadowRoot.querySelector('#editButton'));

          flush();

          editDialog = initializeEditDialog(page);

          // Change name of printer and save the change.
          const nameField =
              editDialog.shadowRoot.querySelector('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.dispatchEvent(
              new CustomEvent('input', {bubbles: true, composed: true}));

          flush();

          clickButton(editDialog.shadowRoot.querySelector('.action-button'));

          return cupsPrintersBrowserProxy.whenCalled('updateCupsPrinter');
        })
        .then(() => {
          assertEquals(expectedName, editDialog.activePrinter.printerName);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });

  test('ReconfigureSavedPrinter', function() {
    const expectedName = 'edited name';
    const expectedAddress = '1.1.1.1';

    let savedPrinterEntries = null;
    let editDialog = null;

    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries = getPrinterEntries(savedPrintersElement);

          // Edit the first entry.
          clickButton(savedPrinterEntries[0].shadowRoot.querySelector(
              '.icon-more-vert'));
          clickButton(
              savedPrintersElement.shadowRoot.querySelector('#editButton'));

          flush();

          editDialog = initializeEditDialog(page);

          const nameField =
              editDialog.shadowRoot.querySelector('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.dispatchEvent(
              new CustomEvent('input', {bubbles: true, composed: true}));

          const addressField =
              editDialog.shadowRoot.querySelector('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;
          addressField.dispatchEvent(
              new CustomEvent('input', {bubbles: true, composed: true}));

          assertFalse(
              editDialog.shadowRoot.querySelector('.cancel-button').hidden);
          assertFalse(
              editDialog.shadowRoot.querySelector('.action-button').hidden);

          flush();

          clickButton(editDialog.shadowRoot.querySelector('.action-button'));

          return cupsPrintersBrowserProxy.whenCalled('reconfigureCupsPrinter');
        })
        .then(() => {
          assertEquals(expectedName, editDialog.activePrinter.printerName);
          assertEquals(
              expectedAddress, editDialog.activePrinter.printerAddress);

          // Mimic changes to |cupsPrintersBrowserProxy.printerList|.
          printerList[0].printerName = expectedName;
          printerList[0].printerAddress = expectedAddress;

          verifyPrintersList(savedPrinterEntries, printerList);
        });
  });

  test('SavedPrintersSearchTermFiltersCorrectPrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

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
  });

  test('SavedPrintersNoSearchFound', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

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
  });

  test('NavigateSavedPrintersList', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(async () => {
          // Wait for saved printers to populate.
          flush();
          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);
          const printerEntryList =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');
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
    params.append('settingId', '1401');
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS, params);

    flush();

    const savedPrinters =
        page.shadowRoot.querySelector('settings-cups-saved-printers');
    const printerEntry = savedPrinters &&
        savedPrinters.shadowRoot.querySelector('settings-cups-printers-entry');
    const deepLinkElement =
        printerEntry && printerEntry.shadowRoot.querySelector('#moreActions');
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

    savedPrintersElement =
        page.shadowRoot.querySelector('settings-cups-saved-printers');
    assertTrue(!!savedPrintersElement);

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
          expectedPrinterIcon = 'os-settings:printer-status-green';
          break;
        case 'id2':
          expectedPrinterIcon = 'os-settings:printer-status-red';
          break;
        case 'id3':
          expectedPrinterIcon = 'os-settings:printer-status-grey';
          break;
        default:
          assertNotReached();
      }
      assertEquals(
          expectedPrinterIcon,
          entry.shadowRoot.querySelector('#printerStatusIcon').icon);
    }

    // Removing the printers should also remove their cache entry.
    await removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement);
    assertFalse(printerStatusReasonCache.has('id1'));
    assertFalse(printerStatusReasonCache.has('id2'));
  });

  test('SavedPrintersStatusPolling', async () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1'),
    ]);
    await flushTasks();
    assertEquals(
        1, cupsPrintersBrowserProxy.getCallCount('requestPrinterStatusUpdate'));

    // Set up the timer to control the delay timers.
    const mockTimer = new MockTimer();
    mockTimer.install();

    // Kick start the printer status query polling and track the number of times
    // the printer status query is triggered.
    page.shadowRoot.querySelector('settings-cups-saved-printers')
        .startPrinterStatusQueryTimerForTesting();

    // Advance the timer only half of the min delay and verify no query is made.
    mockTimer.tick(PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS[0] / 2);
    assertEquals(
        1, cupsPrintersBrowserProxy.getCallCount('requestPrinterStatusUpdate'));

    // Now advance the timer by double the max delay and verify at least one
    // more query request is made.
    mockTimer.tick(PRINTER_STATUS_QUERY_SHORT_DELAY_RANGE_MS[1] * 2);
    assertGE(
        cupsPrintersBrowserProxy.getCallCount('requestPrinterStatusUpdate'), 2);

    mockTimer.uninstall();
  });

  test('ShowMoreButtonIsInitiallyHiddenAndANewPrinterIsAdded', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is hidden because printer list
          // length is <= 3.
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Newly added printers will always be visible and inserted to the
          // top of the list.
          addNewSavedPrinter(createCupsPrinterInfo('test3', '3', 'id3'));
          const expectedVisiblePrinters = [
            createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ];
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          // Assert that the Show more button is still hidden because all newly
          // added printers are visible.
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
        });
  });

  test('PressShowMoreButton', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Click on the Show more button.
          clickButton(
              savedPrintersElement.shadowRoot.querySelector('#show-more-icon'));
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
          // Clicking on the Show more button reveals all hidden printers.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
            createPrinterListEntry('test3', '3', 'id3', PrinterType.SAVED),
          ]);
        });
  });

  test('ShowMoreButtonIsInitiallyShownAndWithANewPrinterAdded', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
      createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
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
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndRemovePrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 5 total printers but only 3 printers are visible and 2
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
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
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
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
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndSearchQueryFiltersCorrectly', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('google3', '5', 'id5'),
      createCupsPrinterInfo('google4', '6', 'id6'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 6 total printers but only 3 printers are visible and 3
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('google3', '5', 'id5', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Set search term to 'google' and expect 4 visible printers.
          let searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          flush();
          verifySearchQueryResults(
              savedPrintersElement,
              [
                createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
                createPrinterListEntry(
                    'google2', '4', 'id4', PrinterType.SAVED),
                createPrinterListEntry(
                    'google3', '5', 'id5', PrinterType.SAVED),
                createPrinterListEntry(
                    'google4', '6', 'id6', PrinterType.SAVED),
              ],
              searchTerm);
          // Having a search term should hide the Show more button.
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Search for a term with no matching printers. Expect Show more
          // button to still be hidden.
          searchTerm = 'noSearchFound';
          savedPrintersElement.searchTerm = searchTerm;
          flush();
          verifySearchQueryResults(savedPrintersElement, [], searchTerm);

          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

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
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

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
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
        });
  });

  test('ShowMoreButtonAddAndRemovePrinters', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('google', '3', 'id3'),
      createCupsPrinterInfo('google2', '4', 'id4'),
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Add a new printer and expect it to be at the top of the list.
          addNewSavedPrinter(createCupsPrinterInfo('newPrinter', '5', 'id5'));
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('newPrinter', '5', 'id5', PrinterType.SAVED),
            createPrinterListEntry('google', '3', 'id3', PrinterType.SAVED),
            createPrinterListEntry('google2', '4', 'id4', PrinterType.SAVED),
            createPrinterListEntry('test1', '1', 'id1', PrinterType.SAVED),
          ]);
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
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
          assertTrue(!!savedPrintersElement.shadowRoot.querySelector(
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
          assertFalse(!!savedPrintersElement.shadowRoot.querySelector(
              '#show-more-container'));
        });
  });

  test('RecordUserActionMetric', function() {
    const fakeMetricsPrivate = new FakeMetricsPrivate();
    chrome.metricsPrivate = fakeMetricsPrivate;

    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsSavedPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          savedPrintersElement =
              page.shadowRoot.querySelector('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          // Remove the first saved printer then verify the action is recorded.
          return removePrinter(
              cupsPrintersBrowserProxy, savedPrintersElement, /*index=*/ 0);
        })
        .then(() => {
          assertEquals(
              1,
              fakeMetricsPrivate.countMetricValue(
                  'Printing.CUPS.SettingsUserAction',
                  PrinterSettingsUserAction.REMOVE_PRINTER));

          // Click the next printer's Edit button then verify the action is
          // recorded.
          const savedPrinterEntries = getPrinterEntries(savedPrintersElement);
          clickButton(savedPrinterEntries[0].shadowRoot.querySelector(
              '.icon-more-vert'));
          clickButton(
              savedPrintersElement.shadowRoot.querySelector('#editButton'));
          assertEquals(
              1,
              fakeMetricsPrivate.countMetricValue(
                  'Printing.CUPS.SettingsUserAction',
                  PrinterSettingsUserAction.EDIT_PRINTER));
        });
  });
});

suite('CupsNearbyPrintersTests', function() {
  let page = null;
  let nearbyPrintersElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type{!HtmlElement} */
  let printerEntryListTestElement = null;

  /** @type {!NetworkStateProperties|undefined} */
  let wifi1;


  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    CupsPrintersBrowserProxyImpl.setInstanceForTesting(
        cupsPrintersBrowserProxy);

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = ConnectionStateType.kOnline;

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);

    flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    nearbyPrintersElement = null;
    page = null;
  });

  test('nearbyPrintersSuccessfullyPopulates', function() {
    const automaticPrinterList = [
      createCupsPrinterInfo('test1', '1', 'id1'),
      createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      createCupsPrinterInfo('test3', '3', 'id3'),
      createCupsPrinterInfo('test4', '4', 'id4'),
    ];

    return flushTasks().then(() => {
      nearbyPrintersElement =
          page.shadowRoot.querySelector('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Assert that no printers have been detected.
      let nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
      assertEquals(0, nearbyPrinterEntries.length);

      // Simuluate finding nearby printers.
      webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      flush();

      nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

      const expectedPrinterList =
          automaticPrinterList.concat(discoveredPrinterList);
      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('nearbyPrintersSortOrderAutoFirstThenDiscovered', function() {
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

    return flushTasks().then(() => {
      nearbyPrintersElement =
          page.shadowRoot.querySelector('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Simuluate finding nearby printers.
      webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      flush();

      const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('addingAutomaticPrinterIsSuccessful', function() {
    const automaticPrinterList = [createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList = [];

    let addButton = null;

    return flushTasks()
        .then(() => {
          nearbyPrintersElement =
              page.shadowRoot.querySelector('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          flush();

          // Requery and assert that the newly detected printer automatic
          // printer has the correct button.
          const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].shadowRoot.querySelector(
              '.save-printer-button'));

          // Add an automatic printer and assert that that the toast
          // notification is shown.
          addButton = nearbyPrinterEntries[0].shadowRoot.querySelector(
              '.save-printer-button');
          clickButton(addButton);
          // Add button should be disabled during setup.
          assertTrue(addButton.disabled);

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          assertFalse(addButton.disabled);
          const expectedToastMessage =
              'Added ' + automaticPrinterList[0].printerName;
          verifyErrorToastMessage(
              expectedToastMessage,
              page.shadowRoot.querySelector('#errorToast'));
        });
  });

  test('NavigateNearbyPrinterList', function() {
    const discoveredPrinterList = [
      createCupsPrinterInfo('first', '3', 'id3'),
      createCupsPrinterInfo('second', '4', 'id4'),
      createCupsPrinterInfo('third', '2', 'id5'),
    ];
    return flushTasks().then(async () => {
      nearbyPrintersElement =
          page.shadowRoot.querySelector('settings-cups-nearby-printers');

      // Block so that FocusRowBehavior.attached can run.
      await waitAfterNextRender(nearbyPrintersElement);

      assertTrue(!!nearbyPrintersElement);
      // Simuluate finding nearby printers.
      webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);
      flush();

      // Wait one more time to ensure that async setup in FocusRowBehavior has
      // executed.
      await waitAfterNextRender(nearbyPrintersElement);
      const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
      const printerEntryList =
          nearbyPrintersElement.shadowRoot.querySelector('#printerEntryList');

      nearbyPrinterEntries[0].shadowRoot.querySelector('#entry').focus();
      assertEquals(
          nearbyPrinterEntries[0].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());
      // Ensure that we can navigate through items in a row
      getDeepActiveElement().dispatchEvent(arrowRightEvent);
      assertEquals(
          nearbyPrinterEntries[0].shadowRoot.querySelector(
              '#setupPrinterButton'),
          getDeepActiveElement());
      getDeepActiveElement().dispatchEvent(arrowLeftEvent);
      assertEquals(
          nearbyPrinterEntries[0].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());

      // Ensure that we can navigate through printer rows
      printerEntryList.dispatchEvent(arrowDownEvent);
      assertEquals(
          nearbyPrinterEntries[1].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());
      printerEntryList.dispatchEvent(arrowDownEvent);
      assertEquals(
          nearbyPrinterEntries[2].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());
      printerEntryList.dispatchEvent(arrowUpEvent);
      assertEquals(
          nearbyPrinterEntries[1].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());
      printerEntryList.dispatchEvent(arrowUpEvent);
      assertEquals(
          nearbyPrinterEntries[0].shadowRoot.querySelector('#entry'),
          getDeepActiveElement());
    });
  });

  test('addingDiscoveredPrinterIsSuccessful', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList = [createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;
    let setupButton = null;

    return flushTasks()
        .then(() => {
          nearbyPrintersElement =
              page.shadowRoot.querySelector('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          flush();

          // Requery and assert that a newly detected discovered printer has
          // the correct icon button.
          const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].shadowRoot.querySelector(
              '#setupPrinterButton'));

          // Force a failure with adding a discovered printer.
          cupsPrintersBrowserProxy.setAddDiscoveredPrinterFailure(
              discoveredPrinterList[0]);

          // Assert that clicking on the setup button shows the advanced
          // configuration dialog.
          setupButton = nearbyPrinterEntries[0].shadowRoot.querySelector(
              '#setupPrinterButton');
          clickButton(setupButton);
          // Setup button should be disabled during setup.
          assertTrue(setupButton.disabled);

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          assertFalse(setupButton.disabled);

          flush();
          const addDialog = page.shadowRoot.querySelector('#addPrinterDialog');
          manufacturerDialog = addDialog.shadowRoot.querySelector(
              'add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(() => {
          const addButton =
              manufacturerDialog.shadowRoot.querySelector('#addPrinterButton');
          assertTrue(addButton.disabled);

          // Populate the manufacturer and model fields to enable the Add
          // button.
          manufacturerDialog.shadowRoot.querySelector('#manufacturerDropdown')
              .value = 'make';
          const modelDropdown =
              manufacturerDialog.shadowRoot.querySelector('#modelDropdown');
          modelDropdown.value = 'model';

          clickButton(addButton);
          return cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
        })
        .then(() => {
          // Assert that the toast notification is shown and has the expected
          // message when adding a discovered printer.
          const expectedToastMessage =
              'Added ' + discoveredPrinterList[0].printerName;
          verifyErrorToastMessage(
              expectedToastMessage,
              page.shadowRoot.querySelector('#errorToast'));
        });
  });

  test('NetworkConnectedButNoInternet', function() {
    // Simulate connecting to a network with no internet connection.
    wifi1.connectionState = ConnectionStateType.kConnected;
    page.onActiveNetworksChanged([wifi1]);
    flush();

    return flushTasks().then(() => {
      // We require internet to be able to add a new printer. Connecting to
      // a network without connectivity should be equivalent to not being
      // connected to a network.
      assertTrue(!!page.shadowRoot.querySelector('#cloudOffIcon'));
      assertTrue(!!page.shadowRoot.querySelector('#connectionMessage'));
      assertTrue(
          !!page.shadowRoot.querySelector('#addManualPrinterIcon').disabled);
    });
  });

  test('checkNetworkConnection', function() {
    // Simulate disconnecting from a network.
    wifi1.connectionState = ConnectionStateType.kNotConnected;
    page.onActiveNetworksChanged([wifi1]);
    flush();

    return flushTasks()
        .then(() => {
          // Expect offline text to show up when no internet is
          // connected.
          assertTrue(!!page.shadowRoot.querySelector('#cloudOffIcon'));
          assertTrue(!!page.shadowRoot.querySelector('#connectionMessage'));
          assertTrue(!!page.shadowRoot.querySelector('#addManualPrinterIcon')
                           .disabled);

          // Simulate connecting to a network with connectivity.
          wifi1.connectionState = ConnectionStateType.kOnline;
          page.onActiveNetworksChanged([wifi1]);
          flush();

          return flushTasks();
        })
        .then(() => {
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

          nearbyPrintersElement =
              page.shadowRoot.querySelector('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          printerEntryListTestElement =
              nearbyPrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');
          assertTrue(!!printerEntryListTestElement);

          const nearbyPrinterEntries = getPrinterEntries(nearbyPrintersElement);

          const expectedPrinterList =
              automaticPrinterList.concat(discoveredPrinterList);
          verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
        });
  });

  test('NearbyPrintersSearchTermFiltersCorrectPrinters', function() {
    const discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('test2', 'printerAddress2', 'printerId2'),
      createCupsPrinterInfo('google', 'printerAddress3', 'printerId3'),
    ];

    return flushTasks().then(() => {
      nearbyPrintersElement =
          page.shadowRoot.querySelector('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.shadowRoot.querySelector('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

      // Simuluate finding nearby printers.
      webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      flush();

      verifyVisiblePrinters(printerEntryListTestElement, [
        createPrinterListEntry(
            'google', 'printerAddress3', 'printerId3', PrinterType.DISCOVERD),
        createPrinterListEntry(
            'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERD),
        createPrinterListEntry(
            'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERD),
      ]);

      let searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      flush();

      // Filtering "google" should result in one visible entry and two hidden
      // entries.
      verifySearchQueryResults(
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress3', 'printerId3',
                                     PrinterType.DISCOVERD)],
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
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2',
                PrinterType.DISCOVERD),
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
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2',
                PrinterType.DISCOVERD),
            createPrinterListEntry(
                'test3', 'printerAddress4', 'printerId4',
                PrinterType.DISCOVERD),
          ],
          searchTerm);
    });
  });

  test('NearbyPrintersNoSearchFound', function() {
    const discoveredPrinterList = [
      createCupsPrinterInfo('test1', 'printerAddress1', 'printerId1'),
      createCupsPrinterInfo('google', 'printerAddress2', 'printerId2'),
    ];

    return flushTasks().then(() => {
      nearbyPrintersElement =
          page.shadowRoot.querySelector('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.shadowRoot.querySelector('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

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
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERED)],
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
          nearbyPrintersElement, [createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERD)],
          searchTerm);
    });
  });
});

suite('CupsEnterprisePrintersTests', function() {
  let page = null;
  let enterprisePrintersElement = null;

  /** @type{!HtmlElement} */
  let printerEntryListTestElement = null;

  /** @type {?settings.TestCupsPrintersBrowserProxy} */
  let cupsPrintersBrowserProxy = null;

  /** @type {?Array<!CupsPrinterInfo>} */
  let printerList = null;

  setup(function() {
    cupsPrintersBrowserProxy = new TestCupsPrintersBrowserProxy();

    PolymerTest.clearBody();
    Router.getInstance().navigateTo(routes.CUPS_PRINTERS);
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    enterprisePrintersElement = null;
    printerList = null;
    page = null;
  });


  function createCupsPrinterPage(printers) {
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
  test('EnterprisePrinters', () => {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', /*isManaged=*/ true),
      createCupsPrinterInfo('test2', '2', 'id2', /*isManaged=*/ true),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          flush();

          enterprisePrintersElement = page.shadowRoot.querySelector(
              'settings-cups-enterprise-printers');
          printerEntryListTestElement =
              enterprisePrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('test1', '1', 'id1', PrinterType.ENTERPRISE),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.ENTERPRISE),
          ]);
        });
  });

  // Verifies that enterprise printers are not editable.
  test('EnterprisePrinterDialog', function() {
    const expectedName = 'edited name';
    let editDialog = null;

    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', true),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList')
        .then(() => {
          // Wait for enterprise printers to populate.
          flush();

          enterprisePrintersElement = page.shadowRoot.querySelector(
              'settings-cups-enterprise-printers');
          assertTrue(!!enterprisePrintersElement);

          const enterprisePrinterEntries =
              getPrinterEntries(enterprisePrintersElement);

          // Users are not allowed to remove enterprise printers.
          const removeButton =
              enterprisePrintersElement.shadowRoot.querySelector(
                  '#removeButton');
          assertTrue(removeButton.disabled);

          clickButton(enterprisePrinterEntries[0].shadowRoot.querySelector(
              '.icon-more-vert'));
          clickButton(enterprisePrintersElement.shadowRoot.querySelector(
              '#viewButton'));

          flush();

          editDialog = initializeEditDialog(page);

          const nameField =
              editDialog.shadowRoot.querySelector('.printer-name-input');
          assertTrue(!!nameField);
          assertEquals('test1', nameField.value);
          assertTrue(nameField.readonly);

          assertTrue(
              editDialog.shadowRoot.querySelector('#printerAddress').readonly);
          assertTrue(
              editDialog.shadowRoot.querySelector('.md-select').disabled);
          assertTrue(
              editDialog.shadowRoot.querySelector('#printerQueue').readonly);
          assertTrue(
              editDialog.shadowRoot.querySelector('#printerPPDManufacturer')
                  .readonly);

          // The "specify PDD" section should be hidden.
          assertTrue(editDialog.shadowRoot.querySelector('.browse-button')
                         .parentElement.hidden);
          assertTrue(editDialog.shadowRoot.querySelector('#ppdLabel').hidden);

          // Save and Cancel buttons should be hidden. Close button should be
          // visible.
          assertTrue(
              editDialog.shadowRoot.querySelector('.cancel-button').hidden);
          assertTrue(
              editDialog.shadowRoot.querySelector('.action-button').hidden);
          assertFalse(
              editDialog.shadowRoot.querySelector('.close-button').hidden);
        });
  });

  test('PressShowMoreButton', function() {
    createCupsPrinterPage([
      createCupsPrinterInfo('test1', '1', 'id1', true),
      createCupsPrinterInfo('test2', '2', 'id2', true),
      createCupsPrinterInfo('test3', '3', 'id3', true),
      createCupsPrinterInfo('test4', '4', 'id4', true),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsEnterprisePrintersList')
        .then(() => {
          // Wait for enterprise printers to populate.
          flush();

          enterprisePrintersElement = page.shadowRoot.querySelector(
              'settings-cups-enterprise-printers');
          assertTrue(!!enterprisePrintersElement);

          const printerEntryListTestElement =
              enterprisePrintersElement.shadowRoot.querySelector(
                  '#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            createPrinterListEntry('test1', '1', 'id1', PrinterType.ENTERPRISE),
            createPrinterListEntry('test2', '2', 'id2', PrinterType.ENTERPRISE),
            createPrinterListEntry('test3', '3', 'id3', PrinterType.ENTERPRISE),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!enterprisePrintersElement.shadowRoot.querySelector(
              '#show-more-container'));

          // Click on the Show more button.
          clickButton(enterprisePrintersElement.shadowRoot.querySelector(
              '#show-more-icon'));
          assertFalse(!!enterprisePrintersElement.shadowRoot.querySelector(
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
});
