// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrinterStatusReason, PrinterType} from 'chrome://os-settings/chromeos/lazy_load.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
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
    printerEntryTestElement.printerStatusReasonCache = new Map();
    document.body.appendChild(printerEntryTestElement);
  });

  teardown(function() {
    printerEntryTestElement.remove();
    printerEntryTestElement = null;
  });

  test('initializePrinterEntry', function() {
    const expectedPrinterName = 'Test name';

    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);
    assertEquals(
        expectedPrinterName,
        printerEntryTestElement.shadowRoot.querySelector('.entry-text')
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
    const printerStatusReasonCache = new Map();
    printerStatusReasonCache.set('id1', PrinterStatusReason.NO_ERROR);
    printerStatusReasonCache.set('id2', PrinterStatusReason.OUT_OF_PAPER);
    printerStatusReasonCache.set(
        'id3', PrinterStatusReason.PRINTER_UNREACHABLE);
    printerStatusReasonCache.set('id4', PrinterStatusReason.UNKNOWN_REASON);

    printerEntryTestElement.printerStatusReasonCache = printerStatusReasonCache;
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.SAVED);

    const printerStatusIcon =
        printerEntryTestElement.shadowRoot.querySelector('#printerStatusIcon');
    const printerSubtext =
        printerEntryTestElement.shadowRoot.querySelector('#printerSubtext');

    // Start at the unknown state.
    assertEquals('os-settings:printer-status-grey', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent.trim());

    // Set to an low severity error status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id2');
    assertEquals('os-settings:printer-status-orange', printerStatusIcon.icon);
    assertEquals(
        loadTimeData.getString('printerStatusOutOfPaper'),
        printerSubtext.textContent.trim());

    // Set to a good status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id1');
    assertEquals('os-settings:printer-status-green', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent.trim());

    // Set to a high severity error status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id3');
    assertEquals('os-settings:printer-status-red', printerStatusIcon.icon);
    assertEquals(
        loadTimeData.getString('printerStatusPrinterUnreachable'),
        printerSubtext.textContent.trim());

    // Set to unknown status reason.
    printerEntryTestElement.set('printerEntry.printerInfo.printerId', 'id4');
    assertEquals('os-settings:printer-status-green', printerStatusIcon.icon);
    assertEquals('', printerSubtext.textContent.trim());

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
