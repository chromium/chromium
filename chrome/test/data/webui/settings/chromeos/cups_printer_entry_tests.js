// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {PrinterType} from 'chrome://os-settings/chromeos/lazy_load.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

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

  Polymer.dom.flush();

  verifyVisiblePrinters(printerEntryListTestElement, expectedVisiblePrinters);
  verifyFilteredPrinters(printerEntryListTestElement, searchTerm);

  if (expectedVisiblePrinters.length) {
    assertTrue(printerEntryListTestElement.$$('#no-search-results').hidden);
  } else {
    assertFalse(printerEntryListTestElement.$$('#no-search-results').hidden);
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
      printerStatus: '',
    },
    printerType: printerType,
  };
}

suite('CupsPrintersEntry', function() {
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
    assertFalse(!!printerEntryTestElement.$$('.icon-more-vert'));

    Polymer.dom.flush();

    // Three dot menu should be visible when |printerType| is set to
    // PrinterType.SAVED.
    assertTrue(!!printerEntryTestElement.$$('.icon-more-vert'));
  });

  test('disableButtonWhenSavingPrinter', function() {
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.DISCOVERED);
    Polymer.dom.flush();
    const setupButton = printerEntryTestElement.$$('#setupPrinterButton');
    assertFalse(setupButton.disabled);
    printerEntryTestElement.savingPrinter = true;
    assertTrue(setupButton.disabled);

    printerEntryTestElement.savingPrinter = false;
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.AUTOMATIC);
    Polymer.dom.flush();
    const automaticButton =
        printerEntryTestElement.$$('#automaticPrinterButton');
    assertFalse(automaticButton.disabled);
    printerEntryTestElement.savingPrinter = true;
    assertTrue(automaticButton.disabled);

    printerEntryTestElement.savingPrinter = false;
    printerEntryTestElement.printerEntry =
        createPrinterEntry(PrinterType.PRINTSERVER);
    Polymer.dom.flush();
    const saveButton = printerEntryTestElement.$$('#savePrinterButton');
    assertFalse(saveButton.disabled);
    printerEntryTestElement.savingPrinter = true;
    assertTrue(saveButton.disabled);
  });
});
