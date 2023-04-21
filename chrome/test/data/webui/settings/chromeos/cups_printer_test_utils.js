// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrinterOnlineState} from 'chrome://os-settings/chromeos/lazy_load.js';

/**
 * @param {string} printerName
 * @param {string} printerAddress
 * @param {string} printerId
 * @param {boolean} isManaged
 * @return {!CupsPrinterInfo}
 * @private
 */
export function createCupsPrinterInfo(
    printerName, printerAddress, printerId, isManaged = false) {
  const printer = {
    isManaged: isManaged,
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress: printerAddress,
    printerDescription: '',
    printerId: printerId,
    printerMakeAndModel: '',
    printerName: printerName,
    printerOnlineState: PrinterOnlineState.UNKNOWN,
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
export function createPrinterListEntry(
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
      printerOnlineState: PrinterOnlineState.UNKNOWN,
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
export function getPrinterEntries(printersElement) {
  const entryList =
      printersElement.shadowRoot.querySelector('#printerEntryList');
  return entryList.querySelectorAll(
      'settings-cups-printers-entry:not([hidden])');
}
