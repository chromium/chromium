// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {CupsPrinterInfo, PrinterListEntry, SettingsCupsPrintersEntryElement} from 'chrome://os-settings/lazy_load.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

export function createCupsPrinterInfo(
    printerName: string, printerAddress: string, printerId: string,
    isManaged: boolean = false): CupsPrinterInfo {
  const printer = {
    isManaged,
    ppdManufacturer: '',
    ppdModel: '',
    printerAddress,
    printerDescription: '',
    printerId,
    printerMakeAndModel: '',
    printerName,
    printerPPDPath: '',
    printerPpdReference: {
      userSuppliedPpdUrl: '',
      effectiveMakeAndModel: '',
      autoconf: false,
    },
    printerProtocol: 'ipp',
    printerQueue: 'moreinfohere',
    printServerUri: '',
  };
  return printer;
}

/**
 * Helper function that creates a new PrinterListEntry.
 */
export function createPrinterListEntry(
    printerName: string, printerAddress: string, printerId: string,
    printerType: number): PrinterListEntry {
  const entry = {
    printerInfo: {
      isManaged: false,
      ppdManufacturer: '',
      ppdModel: '',
      printerAddress,
      printerDescription: '',
      printerId,
      printerMakeAndModel: '',
      printerName,
      printerPPDPath: '',
      printerPpdReference: {
        userSuppliedPpdUrl: '',
        effectiveMakeAndModel: '',
        autoconf: false,
      },
      printerProtocol: 'ipp',
      printerQueue: 'moreinfohere',
      printServerUri: '',
    },
    printerType,
  };
  return entry;
}

/**
 * Helper method to pull an array of CupsPrinterEntry out of a
 * |printersElement|.
 */
export function getPrinterEntries(printersElement: HTMLElement):
    NodeListOf<SettingsCupsPrintersEntryElement> {
  const entryList =
      printersElement.shadowRoot!.querySelector('#printerEntryList');
  assertTrue(!!entryList);
  return entryList.querySelectorAll(
      'settings-cups-printers-entry:not([hidden])');
}
