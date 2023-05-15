// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrinterStatusReason, PrinterType} from 'chrome://os-settings/chromeos/lazy_load.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

/**
 * Helper function to verify that printers in |printerListEntries| that contain
 * |searchTerm| are not in |hiddenEntries|.
 * @param {!Element} printerEntryListTestElement
 * @param {string} searchTerm
 */
function verifyFilteredPrinters(printerEntryListTestElement, searchTerm) {
  const printerListEntries = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
          'settings-cups-printers-entry'));
  const hiddenEntries = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
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
  const actualPrinterList = Array.from(
      printerEntryListTestElement.$.printerEntryList.querySelectorAll(
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
 * @param {!Element} printerEntryListTestElement
 * @param {!Array<!PrinterListEntry>} expectedVisiblePrinters
 * @param {string} searchTerm
 */
function verifySearchQueryResults(
    printerEntryListTestElement, expectedVisiblePrinters, searchTerm) {
  printerEntryListTestElement.searchTerm = searchTerm;

  flush();

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(printerEntryListTestElement.shadowRoot
                   .querySelector('#no-search-results')
                   .hidden);
  } else {
    assertFalse(printerEntryListTestElement.shadowRoot
                    .querySelector('#no-search-results')
                    .hidden);
  }
}

/**
 * Returns a printer entry with the given |printerType|.
 * @param {!PrinterType} printerType
 * @return {!PrinterListEntry}
 */
function createPrinterEntry(printerType) {
  return {
    printerInfo: {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: 'test:123',
      printerDescription: '',
      printerId: 'id_123',
      printerMakeAndModel: '',
      printerName: 'Test name',
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatusReason: PrinterStatusReason.UNKNOWN_REASON,
    },
    printerType: printerType,
  };
}

suite('CupsPrinterEntry', function() {
  /** A printer list entry created before each test. */
  let printerEntryTestElement = null;

  setup(function() {
    PolymerTest.clearBody();
    printerEntryTestElement =
        document.createElement('settings-cups-printers-entry');
    assertTrue(!!printerEntryTestElement);
    document.body.appendChild(printerEntryTestElement);
  });

  teardown(function() {
    printerEntryTestElement.remove();
    printerEntryTestElement = null;
  });

  test('initializePrinterEntry', function() {
    const expectedPrinterName = 'Test name';
    const expectedPrinterSubtext = 'Test subtext';

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);
    assertEquals(
        expectedPrinterName,
        printerEntryTestElement.shadowRoot.querySelector('#printerName')
            .textContent.trim());

    assertTrue(
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .hidden);

    // Assert that setting the subtext will make it visible.
    printerEntryTestElement.subtext = expectedPrinterSubtext;
    assertFalse(
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .hidden);

    assertEquals(
        expectedPrinterSubtext,
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext')
            .textContent.trim());
  });

  test('savedPrinterShowsThreeDotMenu', function() {
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    // Assert that three dot menu is not shown before the dom is updated.
    assertFalse(
        !!printerEntryTestElement.shadowRoot.querySelector('.icon-more-vert'));

    flush();

    // Three dot menu should be visible when |printerType| is set to
    // PrinterType.SAVED.
    assertTrue(
        !!printerEntryTestElement.shadowRoot.querySelector('.icon-more-vert'));
  });

  test('disableButtonWhenSavingPrinterOrDisallowedByPolicy', function() {
    const printerTypes = [
      PrinterType.DISCOVERED,
      PrinterType.AUTOMATIC,
      PrinterType.PRINTSERVER,
    ];
    const printerIds = [
      '#setupPrinterButton',
      '#automaticPrinterButton',
      '#savePrinterButton',
    ];
    for (let i = 0; i < printerTypes.length; i++) {
      printerEntryTestElement.printerEntry =
          createPrinterEntry(printerTypes[i]);
      flush();
      const actionButton =
          printerEntryTestElement.shadowRoot.querySelector(printerIds[i]);
      printerEntryTestElement.savingPrinter = true;
      printerEntryTestElement.userPrintersAllowed = true;
      assertTrue(actionButton.disabled);

      printerEntryTestElement.savingPrinter = true;
      printerEntryTestElement.userPrintersAllowed = false;
      assertTrue(actionButton.disabled);

      printerEntryTestElement.savingPrinter = false;
      printerEntryTestElement.userPrintersAllowed = true;
      assertFalse(actionButton.disabled);

      printerEntryTestElement.savingPrinter = false;
      printerEntryTestElement.userPrintersAllowed = false;
      assertTrue(actionButton.disabled);
    }
  });

  // Verify the correct printer status icon is shown based on the printer's
  // status reason.
  test('savedPrinterCorrectPrinterStatusIcon', function() {
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    // Start at the unknown state.
    assertEquals(
        'os-settings:printer-status-grey',
        printerEntryTestElement.shadowRoot.querySelector('#printerStatusIcon')
            .icon);

    // Set to a good status reason.
    printerEntryTestElement.set(
        'printerEntry.printerInfo.printerStatusReason',
        PrinterStatusReason.NO_ERROR);
    assertEquals(
        'os-settings:printer-status-green',
        printerEntryTestElement.shadowRoot.querySelector('#printerStatusIcon')
            .icon);

    // Set to an error status reason.
    printerEntryTestElement.set(
        'printerEntry.printerInfo.printerStatusReason',
        PrinterStatusReason.OUT_OF_PAPER);
    assertEquals(
        'os-settings:printer-status-red',
        printerEntryTestElement.shadowRoot.querySelector('#printerStatusIcon')
            .icon);
  });

  // Verify the printer icon is visible based on the printer's type.
  test('visiblePrinterIconByPrinterType', function() {
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.ENTERPRISE);
    assertFalse(isVisible(printerEntryTestElement.shadowRoot.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.DISCOVERED);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.AUTOMATIC);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot.querySelector(
        '#printerStatusIcon')));

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);
    assertTrue(isVisible(printerEntryTestElement.shadowRoot.querySelector(
        '#printerStatusIcon')));
  });
});
