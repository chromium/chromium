// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cups_printer_test_util', function() {
  /**
   * @param {string} printerName
   * @param {string} printerAddress
   * @param {string} printerId
   * @return {!CupsPrinterInfo}
   * @private
   */
  /* #export */ function createCupsPrinterInfo(
      printerName, printerAddress, printerId) {
    const printer = {
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress: printerAddress,
      printerDescription: '',
      printerId: printerId,
      printerMakeAndModel: '',
      printerName: printerName,
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printerStatus: '',
      printServerUri: '',
    };
    return printer;
  }

  /**
   * Helper function that creates a new PrinterListEntry.
   * @param {string} printerName
   * @param {string} printerAddress
   * @param {string} printerId
   * @param {string} printerType
   * @return {!PrinterListEntry}
   */
  /* #export */ function createPrinterListEntry(
      printerName, printerAddress, printerId, printerType) {
    const entry = {
      printerInfo: {
        ppdManufacturer: '',
        ppdModel: '',
        printerAddress: printerAddress,
        printerDescription: '',
        printerId: printerId,
        printerMakeAndModel: '',
        printerName: printerName,
        printerPPDPath: '',
        printerPpdReference: {
          userSuppliedPpdUrl: '',
          effectiveMakeAndModel: '',
          autoconf: false,
        },
        printerProtocol: 'ipp',
        printerQueue: 'moreinfohere',
        printerStatus: '',
        printServerUri: '',
      },
      printerType: printerType,
    };
    return entry;
  }

  /**
   * Helper method to pull an array of CupsPrinterEntry out of a
   * |printersElement|.
   * @param {!HTMLElement} printersElement
   * @return {!Array<!HTMLElement>}
   * @private
   */
  /* #export */ function getPrinterEntries(printersElement) {
    const entryList = printersElement.$$('#printerEntryList');
    return entryList.querySelectorAll(
        'settings-cups-printers-entry:not([hidden])');
  }
  // #cr_define_end
  return {
    createCupsPrinterInfo: createCupsPrinterInfo,
    getPrinterEntries: getPrinterEntries,
    createPrinterListEntry: createPrinterListEntry,
  };
});
