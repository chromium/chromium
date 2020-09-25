// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {CupsPrintersBrowserProxyImpl,PrinterSetupResult,CupsPrintersEntryManager,PrintServerResult,PrinterType} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {MojoInterfaceProviderImpl, MojoInterfaceProvider} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {TestCupsPrintersBrowserProxy } from './test_cups_printers_browser_proxy.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {createCupsPrinterInfo,createPrinterListEntry} from './cups_printer_test_utils.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {flushTasks} from '../../test_util.m.js';
// #import {getPrinterEntries} from './cups_printer_test_utils.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickThreeDotMenu(printerEntry) {
  // Click on three dot menu on an item entry.
  const threeDot = printerEntry.$$('.icon-more-vert');
  threeDot.click();

  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickAddAutomaticButton(printerEntry) {
  // Click on add button on an item entry.
  const addButton = printerEntry.$$('.save-printer-button');
  assertTrue(!!addButton);
  addButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} printerEntry
 * @private
 */
function clickSetupButton(printerEntry) {
  // Click on setup button on an item entry.
  const setupButton = printerEntry.$$('#setupPrinterButton');
  assertTrue(!!setupButton);
  setupButton.click();
  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} dialog
 * @private
 */
function clickSaveButton(dialog) {
  const saveButton = dialog.$$('.action-button');

  assertTrue(!!saveButton);
  assertTrue(!saveButton.disabled);

  saveButton.click();

  Polymer.dom.flush();
}

/**
 * @param {!HTMLElement} page
 * @return {!HTMLElement}
 * @private
 */
function initializeEditDialog(page) {
  const editDialog = page.$$('#editPrinterDialog');
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
 * @param {!Array<!CupsPrinterInfo>} printerList
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
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
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
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 * @param {string} searchTerm
 */
function verifySearchQueryResults(
    printersElement, expectedVisiblePrinters, searchTerm) {
  const printerEntryListTestElement = printersElement.$$('#printerEntryList');

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(printersElement.$$('#no-search-results').hidden);
  } else {
    assertFalse(printersElement.$$('#no-search-results').hidden);
  }
}

/**
 * Removes a saved printer located at |index|.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @param {number} index
 * @return {!Promise}
 */
function removePrinter(cupsPrintersBrowserProxy, savedPrintersElement, index) {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;
  const savedPrinterEntries =
      cups_printer_test_util.getPrinterEntries(savedPrintersElement);

  clickThreeDotMenu(savedPrinterEntries[index]);
  savedPrintersElement.$$('#removeButton').click();

  return cupsPrintersBrowserProxy.whenCalled('removeCupsPrinter')
      .then(function() {
        // Simulate removing the printer from |cupsPrintersBrowserProxy|.
        printerList.splice(index, 1);

        // Simuluate saved printer changes.
        cr.webUIListenerCallback(
            'on-printers-changed', cupsPrintersBrowserProxy.printerList);
        Polymer.dom.flush();
      });
}

/**
 * Removes all saved printers through recursion.
 * @param {!TestCupsPrintersBrowserProxy} cupsPrintersBrowserProxy
 * @param {!HTMLElement} savedPrintersElement
 * @return {!Promise}
 */
function removeAllPrinters(cupsPrintersBrowserProxy, savedPrintersElement) {
  const printerList = cupsPrintersBrowserProxy.printerList.printerList;
  const savedPrinterEntries =
      cups_printer_test_util.getPrinterEntries(savedPrintersElement);

  if (!printerList.length) {
    return Promise.resolve();
  }

  return removePrinter(
             cupsPrintersBrowserProxy, savedPrintersElement, 0 /* index */)
      .then(test_util.flushTasks)
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
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.CUPS_PRINTERS);
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    savedPrintersElement = null;
    printerList = null;
    page = null;
  });

  /** @param {!Array<!CupsPrinterInfo>} printerList */
  function createCupsPrinterPage(printers) {
    printerList = printers;
    // |cupsPrinterBrowserProxy| needs to have a list of saved printers before
    // initializing the landing page.
    cupsPrintersBrowserProxy.printerList = {printerList: printerList};
    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);

    Polymer.dom.flush();
  }

  /** @param {!CupsPrinterInfo} printer*/
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
    cr.webUIListenerCallback(
        'on-printers-changed', cupsPrintersBrowserProxy.printerList);
    Polymer.dom.flush();
  }

  test('SavedPrintersSuccessfullyPopulates', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          // List component contained by CupsSavedPrinters.
          const savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');

          const printerListEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);

          verifyPrintersList(printerListEntries, printerList);
        });
  });

  test('SuccessfullyRemoveMultipleSavedPrinters', function() {
    const savedPrinterEntries = [];

    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          const entryList =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);
          verifyPrintersList(entryList, printerList);
        });
  });

  test('HideSavedPrintersWhenEmpty', function() {
    // List component contained by CupsSavedPrinters.
    let savedPrintersList = [];
    let savedPrinterEntries = [];

    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrintersList =
              savedPrintersElement.$$('settings-cups-printers-entry-list');
          savedPrinterEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);

          verifyPrintersList(savedPrinterEntries, printerList);

          assertTrue(!!page.$$('#savedPrinters'));

          return removeAllPrinters(
              cupsPrintersBrowserProxy, savedPrintersElement);
        })
        .then(() => {
          assertFalse(!!page.$$('#savedPrinters'));
        });
  });

  test('UpdateSavedPrinter', function() {
    const expectedName = 'edited name';

    let editDialog = null;
    let savedPrinterEntries = null;

    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);

          // Update the printer name of the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          // Change name of printer and save the change.
          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

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
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          savedPrinterEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);

          // Edit the first entry.
          clickThreeDotMenu(savedPrinterEntries[0]);
          savedPrintersElement.$$('#editButton').click();

          Polymer.dom.flush();

          editDialog = initializeEditDialog(page);

          const nameField = editDialog.$$('.printer-name-input');
          assertTrue(!!nameField);
          nameField.value = expectedName;
          nameField.fire('input');

          const addressField = editDialog.$$('#printerAddress');
          assertTrue(!!addressField);
          addressField.value = expectedAddress;
          addressField.fire('input');

          Polymer.dom.flush();

          clickSaveButton(editDialog);

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
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerListEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);
          verifyPrintersList(printerListEntries, printerList);

          let searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          // Filtering "google" should result in one visible entry and two
          // hidden entries.
          verifySearchQueryResults(
              savedPrintersElement,
              [cups_printer_test_util.createPrinterListEntry(
                  'google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);

          // Change the search term and assert that entries are filtered
          // correctly. Filtering "test" should result in three visible entries
          // and one hidden entry.
          searchTerm = 'test';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [
                cups_printer_test_util.createPrinterListEntry(
                    'test1', '1', 'id1', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'test2', '2', 'id2', PrinterType.SAVED),
              ],
              searchTerm);

          // Add more printers and assert that they are correctly filtered.
          addNewSavedPrinter(cups_printer_test_util.createCupsPrinterInfo(
              'test3', '3', 'id3'));
          addNewSavedPrinter(cups_printer_test_util.createCupsPrinterInfo(
              'google2', '6', 'id6'));
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [
                cups_printer_test_util.createPrinterListEntry(
                    'test3', '3', 'id3', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'test1', '1', 'id1', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'test2', '2', 'id2', PrinterType.SAVED)
              ],
              searchTerm);
        });
  });

  test('SavedPrintersNoSearchFound', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerListEntries =
              cups_printer_test_util.getPrinterEntries(savedPrintersElement);
          verifyPrintersList(printerListEntries, printerList);

          let searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          // Filtering "google" should result in one visible entry and three
          // hidden entries.
          verifySearchQueryResults(
              savedPrintersElement,
              [cups_printer_test_util.createPrinterListEntry(
                  'google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);

          // Change search term to something that has no matches.
          searchTerm = 'noSearchFound';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(savedPrintersElement, [], searchTerm);

          // Change search term back to "google" and verify that the No search
          // found message is no longer there.
          searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();

          verifySearchQueryResults(
              savedPrintersElement,
              [cups_printer_test_util.createPrinterListEntry(
                  'google', '4', 'id4', PrinterType.SAVED)],
              searchTerm);
        });
  });

  test('Deep link to saved printers', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
      cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3'),
    ]);

    await cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList');

    const params = new URLSearchParams;
    params.append('settingId', '1401');
    settings.Router.getInstance().navigateTo(
        settings.routes.CUPS_PRINTERS, params);

    Polymer.dom.flush();

    const savedPrinters = page.$$('settings-cups-saved-printers');
    const printerEntry =
        savedPrinters && savedPrinters.$$('settings-cups-printers-entry');
    const deepLinkElement = printerEntry && printerEntry.$$('#moreActions');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'First saved printer menu button should be focused for settingId=1401.');
  });

  test('ShowMoreButtonIsInitiallyHiddenAndANewPrinterIsAdded', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is hidden because printer list
          // length is <= 3.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Newly added printers will always be visible and inserted to the
          // top of the list.
          addNewSavedPrinter(cups_printer_test_util.createCupsPrinterInfo(
              'test3', '3', 'id3'));
          const expectedVisiblePrinters = [
            cups_printer_test_util.createPrinterListEntry(
                'test3', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED)
          ];
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          // Assert that the Show more button is still hidden because all newly
          // added printers are visible.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('PressShowMoreButton', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
      cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Click on the Show more button.
          savedPrintersElement.$$('#show-more-icon').click();
          Polymer.dom.flush();
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
          // Clicking on the Show more button reveals all hidden printers.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test3', '3', 'id3', PrinterType.SAVED)
          ]);
        });
  });

  test('ShowMoreButtonIsInitiallyShownAndWithANewPrinterAdded', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
      cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED),
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Newly added printers will always be visible.
          addNewSavedPrinter(cups_printer_test_util.createCupsPrinterInfo(
              'test5', '5', 'id5'));
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'test5', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is still shown.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndRemovePrinters', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '3', 'id3'),
      cups_printer_test_util.createCupsPrinterInfo('google2', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('google3', '5', 'id5'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 5 total printers but only 3 printers are visible and 2
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google3', '5', 'id5', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Simulate removing 'google' printer.
          removeSavedPrinter('id3');
          // Printer list has 4 elements now, but since the list is still
          // collapsed we should still expect only 3 elements to be visible.
          // Since printers were initially alphabetically sorted, we should
          // expect 'test1' to be the next visible printer.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google3', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Simulate removing 'google2' printer.
          removeSavedPrinter('id4');
          // Printer list has 3 elements now, the Show more button should be
          // hidden.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google3', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED)
          ]);
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonIsShownAndSearchQueryFiltersCorrectly', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '3', 'id3'),
      cups_printer_test_util.createCupsPrinterInfo('google2', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('google3', '5', 'id5'),
      cups_printer_test_util.createCupsPrinterInfo('google4', '6', 'id6'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 6 total printers but only 3 printers are visible and 3
          // are hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google3', '5', 'id5', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Set search term to 'google' and expect 4 visible printers.
          let searchTerm = 'google';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(
              savedPrintersElement,
              [
                cups_printer_test_util.createPrinterListEntry(
                    'google', '3', 'id3', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'google2', '4', 'id4', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'google3', '5', 'id5', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'google4', '6', 'id6', PrinterType.SAVED)
              ],
              searchTerm);
          // Having a search term should hide the Show more button.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Search for a term with no matching printers. Expect Show more
          // button to still be hidden.
          searchTerm = 'noSearchFound';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(savedPrintersElement, [], searchTerm);

          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Change search term and expect new set of visible printers.
          searchTerm = 'test';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          verifySearchQueryResults(
              savedPrintersElement,
              [
                cups_printer_test_util.createPrinterListEntry(
                    'test1', '1', 'id1', PrinterType.SAVED),
                cups_printer_test_util.createPrinterListEntry(
                    'test2', '2', 'id2', PrinterType.SAVED)
              ],
              searchTerm);
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));

          // Remove the search term and expect the collapsed list to appear
          // again.
          searchTerm = '';
          savedPrintersElement.searchTerm = searchTerm;
          Polymer.dom.flush();
          const expectedVisiblePrinters = [
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google3', '5', 'id5', PrinterType.SAVED)
          ];
          verifySearchQueryResults(
              savedPrintersElement, expectedVisiblePrinters, searchTerm);
          verifyVisiblePrinters(
              printerEntryListTestElement, expectedVisiblePrinters);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));
        });
  });

  test('ShowMoreButtonAddAndRemovePrinters', function() {
    createCupsPrinterPage([
      cups_printer_test_util.createCupsPrinterInfo('google', '3', 'id3'),
      cups_printer_test_util.createCupsPrinterInfo('google2', '4', 'id4'),
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ]);
    return cupsPrintersBrowserProxy.whenCalled('getCupsPrintersList')
        .then(() => {
          // Wait for saved printers to populate.
          Polymer.dom.flush();

          savedPrintersElement = page.$$('settings-cups-saved-printers');
          assertTrue(!!savedPrintersElement);

          const printerEntryListTestElement =
              savedPrintersElement.$$('#printerEntryList');

          // There are 4 total printers but only 3 printers are visible and 1 is
          // hidden underneath the Show more section.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED)
          ]);
          // Assert that the Show more button is shown since printer list length
          // is > 3.
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Add a new printer and expect it to be at the top of the list.
          addNewSavedPrinter(cups_printer_test_util.createCupsPrinterInfo(
              'newPrinter', '5', 'id5'));
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'newPrinter', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test1', '1', 'id1', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Now simulate removing printer 'test1'.
          removeSavedPrinter('id1');
          // If the number of visible printers is > 3, removing printers will
          // decrease the number of visible printers until there are only 3
          // visible printers. In this case, we remove 'test1' and now only
          // have 3 visible printers and 1 hidden printer: 'test2'.
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'newPrinter', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google2', '4', 'id4', PrinterType.SAVED)
          ]);
          assertTrue(!!savedPrintersElement.$$('#show-more-container'));

          // Remove another printer and assert that we still have 3 visible
          // printers but now 'test2' is our third visible printer.
          removeSavedPrinter('id4');
          verifyVisiblePrinters(printerEntryListTestElement, [
            cups_printer_test_util.createPrinterListEntry(
                'newPrinter', '5', 'id5', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'google', '3', 'id3', PrinterType.SAVED),
            cups_printer_test_util.createPrinterListEntry(
                'test2', '2', 'id2', PrinterType.SAVED)
          ]);
          // Printer list length is <= 3, Show more button should be hidden.
          assertFalse(!!savedPrintersElement.$$('#show-more-container'));
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

  /** @type {!chromeos.networkConfig.mojom.NetworkStateProperties|undefined} */
  let wifi1;


  setup(function() {
    const mojom = chromeos.networkConfig.mojom;
    cupsPrintersBrowserProxy =
        new printerBrowserProxy.TestCupsPrintersBrowserProxy;

    settings.CupsPrintersBrowserProxyImpl.instance_ = cupsPrintersBrowserProxy;

    // Simulate internet connection.
    wifi1 = OncMojo.getDefaultNetworkState(mojom.NetworkType.kWiFi, 'wifi1');
    wifi1.connectionState = mojom.ConnectionStateType.kOnline;

    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.CUPS_PRINTERS);

    page = document.createElement('settings-cups-printers');
    document.body.appendChild(page);
    assertTrue(!!page);
    page.onActiveNetworksChanged([wifi1]);

    Polymer.dom.flush();
  });

  teardown(function() {
    cupsPrintersBrowserProxy.reset();
    page.remove();
    nearbyPrintersElement = null;
    page = null;
  });

  test('nearbyPrintersSuccessfullyPopulates', function() {
    const automaticPrinterList = [
      cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
      cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
    ];
    const discoveredPrinterList = [
      cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3'),
      cups_printer_test_util.createCupsPrinterInfo('test4', '4', 'id4'),
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Assert that no printers have been detected.
      let nearbyPrinterEntries =
          cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);
      assertEquals(0, nearbyPrinterEntries.length);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      nearbyPrinterEntries =
          cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);

      const expectedPrinterList =
          automaticPrinterList.concat(discoveredPrinterList);
      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('nearbyPrintersSortOrderAutoFirstThenDiscovered', function() {
    const discoveredPrinterA = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameA', 'printerAddress1', 'printerId1');
    const discoveredPrinterB = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameB', 'printerAddress2', 'printerId2');
    const discoveredPrinterC = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameC', 'printerAddress3', 'printerId3');
    const autoPrinterD = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameD', 'printerAddress4', 'printerId4');
    const autoPrinterE = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameE', 'printerAddress5', 'printerId5');
    const autoPrinterF = cups_printer_test_util.createCupsPrinterInfo(
        'printerNameF', 'printerAddress6', 'printerId6');

    // Add printers in a non-alphabetical order to test sorting.
    const automaticPrinterList = [autoPrinterF, autoPrinterD, autoPrinterE];
    const discoveredPrinterList =
        [discoveredPrinterC, discoveredPrinterA, discoveredPrinterB];

    // Expected sort order is to sort automatic printers first then
    // sort discovered printers
    const expectedPrinterList = [
      autoPrinterD, autoPrinterE, autoPrinterF, discoveredPrinterA,
      discoveredPrinterB, discoveredPrinterC
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', automaticPrinterList,
          discoveredPrinterList);

      Polymer.dom.flush();

      const nearbyPrinterEntries =
          cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);

      verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
    });
  });

  test('addingAutomaticPrinterIsSuccessful', function() {
    const automaticPrinterList =
        [cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1')];
    const discoveredPrinterList = [];

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that the newly detected printer automatic
          // printer has the correct button.
          const nearbyPrinterEntries =
              cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('.save-printer-button'));

          // Add an automatic printer and assert that that the toast
          // notification is shown.
          clickAddAutomaticButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          const expectedToastMessage =
              'Added ' + automaticPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('addingDiscoveredPrinterIsSuccessful', function() {
    const automaticPrinterList = [];
    const discoveredPrinterList =
        [cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3')];

    let manufacturerDialog = null;

    return test_util.flushTasks()
        .then(() => {
          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          // Requery and assert that a newly detected discovered printer has
          // the correct icon button.
          const nearbyPrinterEntries =
              cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);
          assertEquals(1, nearbyPrinterEntries.length);
          assertTrue(!!nearbyPrinterEntries[0].$$('#setupPrinterButton'));

          // Force a failure with adding a discovered printer.
          cupsPrintersBrowserProxy.setAddDiscoveredPrinterFailure(
              discoveredPrinterList[0]);

          // Assert that clicking on the setup button shows the advanced
          // configuration dialog.
          clickSetupButton(nearbyPrinterEntries[0]);

          Polymer.dom.flush();

          return cupsPrintersBrowserProxy.whenCalled('addDiscoveredPrinter');
        })
        .then(() => {
          Polymer.dom.flush();
          const addDialog = page.$$('#addPrinterDialog');
          manufacturerDialog =
              addDialog.$$('add-printer-manufacturer-model-dialog');
          assertTrue(!!manufacturerDialog);

          return cupsPrintersBrowserProxy.whenCalled(
              'getCupsPrinterManufacturersList');
        })
        .then(() => {
          const addButton = manufacturerDialog.$$('#addPrinterButton');
          assertTrue(addButton.disabled);

          // Populate the manufacturer and model fields to enable the Add
          // button.
          manufacturerDialog.$$('#manufacturerDropdown').value = 'make';
          const modelDropdown = manufacturerDialog.$$('#modelDropdown');
          modelDropdown.value = 'model';

          assertTrue(!addButton.disabled);

          addButton.click();
          return cupsPrintersBrowserProxy.whenCalled('addCupsPrinter');
        })
        .then(() => {
          // Assert that the toast notification is shown and has the expected
          // message when adding a discovered printer.
          const expectedToastMessage =
              'Added ' + discoveredPrinterList[0].printerName;
          verifyErrorToastMessage(expectedToastMessage, page.$$('#errorToast'));
        });
  });

  test('NetworkConnectedButNoInternet', function() {
    // Simulate connecting to a network with no internet connection.
    wifi1.connectionState =
        chromeos.networkConfig.mojom.ConnectionStateType.kConnected;
    page.onActiveNetworksChanged([wifi1]);
    Polymer.dom.flush();

    return test_util.flushTasks().then(() => {
      // We require internet to be able to add a new printer. Connecting to
      // a network without connectivity should be equivalent to not being
      // connected to a network.
      assertTrue(!!page.$$('#cloudOffIcon'));
      assertTrue(!!page.$$('#connectionMessage'));
      assertTrue(!!page.$$('#addManualPrinterIcon').disabled);
    });
  });

  test('checkNetworkConnection', function() {
    // Simulate disconnecting from a network.
    wifi1.connectionState =
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected;
    page.onActiveNetworksChanged([wifi1]);
    Polymer.dom.flush();

    return test_util.flushTasks()
        .then(() => {
          // Expect offline text to show up when no internet is
          // connected.
          assertTrue(!!page.$$('#cloudOffIcon'));
          assertTrue(!!page.$$('#connectionMessage'));
          assertTrue(!!page.$$('#addManualPrinterIcon').disabled);

          // Simulate connecting to a network with connectivity.
          wifi1.connectionState =
              chromeos.networkConfig.mojom.ConnectionStateType.kOnline;
          page.onActiveNetworksChanged([wifi1]);
          Polymer.dom.flush();

          return test_util.flushTasks();
        })
        .then(() => {
          const automaticPrinterList = [
            cups_printer_test_util.createCupsPrinterInfo('test1', '1', 'id1'),
            cups_printer_test_util.createCupsPrinterInfo('test2', '2', 'id2'),
          ];
          const discoveredPrinterList = [
            cups_printer_test_util.createCupsPrinterInfo('test3', '3', 'id3'),
            cups_printer_test_util.createCupsPrinterInfo('test4', '4', 'id4'),
          ];

          // Simuluate finding nearby printers.
          cr.webUIListenerCallback(
              'on-nearby-printers-changed', automaticPrinterList,
              discoveredPrinterList);

          Polymer.dom.flush();

          nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
          assertTrue(!!nearbyPrintersElement);

          printerEntryListTestElement =
              nearbyPrintersElement.$$('#printerEntryList');
          assertTrue(!!printerEntryListTestElement);

          const nearbyPrinterEntries =
              cups_printer_test_util.getPrinterEntries(nearbyPrintersElement);

          const expectedPrinterList =
              automaticPrinterList.concat(discoveredPrinterList);
          verifyPrintersList(nearbyPrinterEntries, expectedPrinterList);
        });
  });

  test('NearbyPrintersSearchTermFiltersCorrectPrinters', function() {
    const discoveredPrinterList = [
      cups_printer_test_util.createCupsPrinterInfo(
          'test1', 'printerAddress1', 'printerId1'),
      cups_printer_test_util.createCupsPrinterInfo(
          'test2', 'printerAddress2', 'printerId2'),
      cups_printer_test_util.createCupsPrinterInfo(
          'google', 'printerAddress3', 'printerId3'),
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.$$('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      verifyVisiblePrinters(printerEntryListTestElement, [
        cups_printer_test_util.createPrinterListEntry(
            'google', 'printerAddress3', 'printerId3', PrinterType.DISCOVERD),
        cups_printer_test_util.createPrinterListEntry(
            'test1', 'printerAddress1', 'printerId1', PrinterType.DISCOVERD),
        cups_printer_test_util.createPrinterListEntry(
            'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERD)
      ]);

      let searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      // Filtering "google" should result in one visible entry and two hidden
      // entries.
      verifySearchQueryResults(
          nearbyPrintersElement, [cups_printer_test_util.createPrinterListEntry(
                                     'google', 'printerAddress3', 'printerId3',
                                     PrinterType.DISCOVERD)],
          searchTerm);

      // Filtering "test" should result in two visible entries and one hidden
      // entry.
      searchTerm = 'test';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement,
          [
            cups_printer_test_util.createPrinterListEntry(
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            cups_printer_test_util.createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2', PrinterType.DISCOVERD)
          ],
          searchTerm);

      // Add more printers and assert that they are correctly filtered.
      discoveredPrinterList.push(cups_printer_test_util.createCupsPrinterInfo(
          'test3', 'printerAddress4', 'printerId4'));
      discoveredPrinterList.push(cups_printer_test_util.createCupsPrinterInfo(
          'google2', 'printerAddress5', 'printerId5'));

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement,
          [
            cups_printer_test_util.createPrinterListEntry(
                'test1', 'printerAddress1', 'printerId1',
                PrinterType.DISCOVERD),
            cups_printer_test_util.createPrinterListEntry(
                'test2', 'printerAddress2', 'printerId2',
                PrinterType.DISCOVERD),
            cups_printer_test_util.createPrinterListEntry(
                'test3', 'printerAddress4', 'printerId4', PrinterType.DISCOVERD)
          ],
          searchTerm);
    });
  });

  test('NearbyPrintersNoSearchFound', function() {
    const discoveredPrinterList = [
      cups_printer_test_util.createCupsPrinterInfo(
          'test1', 'printerAddress1', 'printerId1'),
      cups_printer_test_util.createCupsPrinterInfo(
          'google', 'printerAddress2', 'printerId2')
    ];

    return test_util.flushTasks().then(() => {
      nearbyPrintersElement = page.$$('settings-cups-nearby-printers');
      assertTrue(!!nearbyPrintersElement);

      printerEntryListTestElement =
          nearbyPrintersElement.$$('#printerEntryList');
      assertTrue(!!printerEntryListTestElement);

      // Simuluate finding nearby printers.
      cr.webUIListenerCallback(
          'on-nearby-printers-changed', [], discoveredPrinterList);

      Polymer.dom.flush();

      let searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      // Set the search term and filter out the printers. Filtering "google"
      // should result in one visible entry and one hidden entries.
      verifySearchQueryResults(
          nearbyPrintersElement, [cups_printer_test_util.createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERED)],
          searchTerm);

      // Change search term to something that has no matches.
      searchTerm = 'noSearchFound';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(nearbyPrintersElement, [], searchTerm);

      // Change search term back to "google" and verify that the No search found
      // message is no longer there.
      searchTerm = 'google';
      nearbyPrintersElement.searchTerm = searchTerm;
      Polymer.dom.flush();

      verifySearchQueryResults(
          nearbyPrintersElement, [cups_printer_test_util.createPrinterListEntry(
                                     'google', 'printerAddress2', 'printerId2',
                                     PrinterType.DISCOVERD)],
          searchTerm);
    });
  });
});
