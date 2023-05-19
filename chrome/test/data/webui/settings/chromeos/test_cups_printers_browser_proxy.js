// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrinterSetupResult, PrintServerResult} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestCupsPrintersBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'addCupsPrinter',
      'addDiscoveredPrinter',
      'getCupsSavedPrintersList',
      'getCupsEnterprisePrintersList',
      'getCupsPrinterManufacturersList',
      'getCupsPrinterModelsList',
      'getPrinterInfo',
      'getPrinterPpdManufacturerAndModel',
      'queryPrintServer',
      'startDiscoveringPrinters',
      'stopDiscoveringPrinters',
      'cancelPrinterSetUp',
      'updateCupsPrinter',
      'removeCupsPrinter',
      'reconfigureCupsPrinter',
      'getEulaUrl',
      'requestPrinterStatusUpdate',
    ]);

    this.printerList = /** @type{?CupsPrintersList} */ ({printerList: []});
    this.printServerPrinters =
        /** @type{?CupsPrintersList}  */ ({printerList: []});
    this.manufacturers =
        /** @type{?ManufacturersInfo} */ ({success: false, manufacturers: []});
    this.models =
        /** @type{?ModelsInfo} */ ({success: false, models: []});
    this.printerInfo = {};
    this.printerPpdMakeModel =
        /** @type{PrinterPpdMakeModel */ ({ppdManufacturer: '', ppdModel: ''});
    this.printerStatusMap = {};

    /**
     * |eulaUrl_| in conjunction with |setEulaUrl| mimics setting the EULA url
     * for a printer.
     * @private {string}
     */
    this.eulaUrl_ = '';

    /**
     * If set, 'getPrinterInfo' will fail and the promise will be reject with
     * this PrinterSetupResult.
     * @private {PrinterSetupResult}
     */
    this.getPrinterInfoResult_ = null;

    /**
     * Contains the result code from querying a print server.
     * @private {PrintServerResult}
     */
    this.queryPrintServerResult_ = null;

    /**
     * If set, 'addDiscoveredPrinter' will fail and the promise will be
     * rejected with this printer.
     * @private {CupsPrinterInfo}
     */
    this.addDiscoveredFailedPrinter_ = null;
  }

  /** @override */
  addCupsPrinter(newPrinter) {
    this.methodCalled('addCupsPrinter', newPrinter);
    return Promise.resolve(PrinterSetupResult.SUCCESS);
  }

  /** @override */
  addDiscoveredPrinter(printerId) {
    this.methodCalled('addDiscoveredPrinter', printerId);
    if (this.addDiscoveredFailedPrinter_ != null) {
      return Promise.reject(this.addDiscoveredFailedPrinter_);
    }
    return Promise.resolve(PrinterSetupResult.SUCCESS);
  }

  /** @override */
  getCupsSavedPrintersList() {
    this.methodCalled('getCupsSavedPrintersList');
    return Promise.resolve(this.printerList);
  }

  /** @override */
  getCupsEnterprisePrintersList() {
    this.methodCalled('getCupsEnterprisePrintersList');
    return Promise.resolve(this.printerList);
  }

  /** @override */
  getCupsPrinterManufacturersList() {
    this.methodCalled('getCupsPrinterManufacturersList');
    return Promise.resolve(this.manufacturers);
  }

  /** @override */
  getCupsPrinterModelsList(manufacturer) {
    this.methodCalled('getCupsPrinterModelsList', manufacturer);
    return Promise.resolve(this.models);
  }

  /** @override */
  getPrinterInfo(newPrinter) {
    this.methodCalled('getPrinterInfo', newPrinter);
    if (this.getPrinterInfoResult_ != null) {
      return Promise.reject(this.getPrinterInfoResult_);
    }
    return Promise.resolve(this.printerInfo);
  }

  /** @override */
  startDiscoveringPrinters() {
    this.methodCalled('startDiscoveringPrinters');
  }

  /** @override */
  stopDiscoveringPrinters() {
    this.methodCalled('stopDiscoveringPrinters');
  }

  /** @override */
  cancelPrinterSetUp(newPrinter) {
    this.methodCalled('cancelPrinterSetUp', newPrinter);
  }

  /** @override */
  updateCupsPrinter(printerId, printerName) {
    this.methodCalled('updateCupsPrinter', [printerId, printerName]);
    return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
  }

  /** @override */
  removeCupsPrinter(printerId, printerName) {
    this.methodCalled('removeCupsPrinter', [printerId, printerName]);
  }

  /** @override */
  getPrinterPpdManufacturerAndModel(printerId) {
    this.methodCalled('getPrinterPpdManufacturerAndModel', printerId);
    return Promise.resolve(this.printerPpdMakeModel);
  }

  /** @override */
  reconfigureCupsPrinter(printer) {
    this.methodCalled('reconfigureCupsPrinter', printer);
    return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
  }

  /** @override */
  getEulaUrl(ppdManufacturer, ppdModel) {
    this.methodCalled('getEulaUrl', [ppdManufacturer, ppdModel]);
    return Promise.resolve(this.eulaUrl_);
  }

  /** @override */
  queryPrintServer(serverUrl) {
    this.methodCalled('queryPrintServer', serverUrl);
    if (this.queryPrintServerResult_ !== PrintServerResult.NO_ERRORS) {
      return Promise.reject(this.queryPrintServerResult_);
    }
    return Promise.resolve(this.printServerPrinters);
  }

  /** @override */
  requestPrinterStatusUpdate(printerId) {
    this.methodCalled('requestPrinterStatusUpdate', printerId);
    return Promise.resolve(this.printerStatusMap[printerId]);
  }

  /** @param {string} eulaUrl */
  setEulaUrl(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  }

  /** @param {PrinterSetupResult} result */
  setGetPrinterInfoResult(result) {
    this.getPrinterInfoResult_ = result;
  }

  /** @param {PrintServerResult} result */
  setQueryPrintServerResult(result) {
    this.queryPrintServerResult_ = result;
  }

  /** @param {!CupsPrinterInfo} printer */
  setAddDiscoveredPrinterFailure(printer) {
    this.addDiscoveredFailedPrinter_ = printer;
  }

  /**
   * @param {string} printerId
   * @param {!PrinterStatusReason} reason
   * @param {!PrinterStatusSeverity} severity
   */
  addPrinterStatus(printerId, reason, severity) {
    this.printerStatusMap[printerId] = {
      printerId: printerId,
      statusReasons: [
        {
          reason: reason,
          severity: severity,
        },
      ],
      timestamp: 0,
    };
  }
}
