// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrinterStatus, PrinterStatusReason, PrinterStatusSeverity, PrintServerResult} from 'chrome://os-settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestCupsPrintersBrowserProxy extends TestBrowserProxy implements
    CupsPrintersBrowserProxy {
  printerList: CupsPrintersList;
  printServerPrinters: CupsPrintersList;
  manufacturers: ManufacturersInfo;
  models: ModelsInfo;
  printerInfo: PrinterMakeModel;
  printerPpdMakeModel: PrinterPpdMakeModel;
  printerStatusMap: {[key: string]: PrinterStatus};
  printerPpdPath: string;
  private eulaUrl: string;
  private getPrinterInfoResult: PrinterSetupResult|null;
  private queryPrintServerResult: PrintServerResult|null;
  private addDiscoveredFailedPrinter: CupsPrinterInfo|null;

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
      'retrieveCupsPrinterPpd',
      'getCupsPrinterPpdPath',
      'openPrintManagementApp',
      'openScanningApp',
    ]);


    this.printerList = {printerList: []};
    this.printServerPrinters = {printerList: []};
    this.manufacturers = {success: false, manufacturers: []};
    this.models = {success: false, models: []};
    this.printerInfo = {
      makeAndModel: '',
      autoconf: false,
      ppdRefUserSuppliedPpdUrl: '',
      ppdRefEffectiveMakeAndModel: '',
      ppdReferenceResolved: false,
    };
    this.printerPpdMakeModel = {ppdManufacturer: '', ppdModel: ''};
    this.printerStatusMap = {};
    this.printerPpdPath = '';
    /**
     * |eulaUrl| in conjunction with |setEulaUrl| mimics setting the EULA url
     * for a printer.
     */
    this.eulaUrl = '';

    /**
     * If set, 'getPrinterInfo' will fail and the promise will be reject with
     * this PrinterSetupResult.
     */
    this.getPrinterInfoResult = null;

    /**
     * Contains the result code from querying a print server.
     */
    this.queryPrintServerResult = null;

    /**
     * If set, 'addDiscoveredPrinter' will fail and the promise will be
     * rejected with this printer.
     */
    this.addDiscoveredFailedPrinter = null;
  }

  addCupsPrinter(newPrinter: CupsPrinterInfo): Promise<PrinterSetupResult> {
    this.methodCalled('addCupsPrinter', newPrinter);
    return Promise.resolve(PrinterSetupResult.SUCCESS);
  }

  addDiscoveredPrinter(printerId: string): Promise<PrinterSetupResult> {
    this.methodCalled('addDiscoveredPrinter', printerId);
    if (this.addDiscoveredFailedPrinter !== null) {
      return Promise.reject(this.addDiscoveredFailedPrinter);
    }
    return Promise.resolve(PrinterSetupResult.SUCCESS);
  }

  getCupsSavedPrintersList(): Promise<CupsPrintersList> {
    this.methodCalled('getCupsSavedPrintersList');
    return Promise.resolve(this.printerList);
  }

  getCupsEnterprisePrintersList(): Promise<CupsPrintersList> {
    this.methodCalled('getCupsEnterprisePrintersList');
    return Promise.resolve(this.printerList);
  }

  getCupsPrinterManufacturersList(): Promise<ManufacturersInfo> {
    this.methodCalled('getCupsPrinterManufacturersList');
    return Promise.resolve(this.manufacturers);
  }

  getCupsPrinterModelsList(manufacturer: string): Promise<ModelsInfo> {
    this.methodCalled('getCupsPrinterModelsList', manufacturer);
    return Promise.resolve(this.models);
  }

  getPrinterInfo(newPrinter: CupsPrinterInfo): Promise<PrinterMakeModel> {
    this.methodCalled('getPrinterInfo', newPrinter);
    if (this.getPrinterInfoResult !== null) {
      return Promise.reject(this.getPrinterInfoResult);
    }
    return Promise.resolve(this.printerInfo);
  }

  startDiscoveringPrinters(): void {
    this.methodCalled('startDiscoveringPrinters');
  }

  stopDiscoveringPrinters(): void {
    this.methodCalled('stopDiscoveringPrinters');
  }

  cancelPrinterSetUp(newPrinter: CupsPrinterInfo): void {
    this.methodCalled('cancelPrinterSetUp', newPrinter);
  }

  updateCupsPrinter(printerId: string, printerName: string):
      Promise<PrinterSetupResult> {
    this.methodCalled('updateCupsPrinter', [printerId, printerName]);
    return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
  }

  removeCupsPrinter(printerId: string, printerName: string): void {
    this.methodCalled('removeCupsPrinter', [printerId, printerName]);
  }

  getPrinterPpdManufacturerAndModel(printerId: string):
      Promise<PrinterPpdMakeModel> {
    this.methodCalled('getPrinterPpdManufacturerAndModel', printerId);
    return Promise.resolve(this.printerPpdMakeModel);
  }

  reconfigureCupsPrinter(printer: CupsPrinterInfo):
      Promise<PrinterSetupResult> {
    this.methodCalled('reconfigureCupsPrinter', printer);
    return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
  }

  getEulaUrl(ppdManufacturer: string, ppdModel: string): Promise<string> {
    this.methodCalled('getEulaUrl', [ppdManufacturer, ppdModel]);
    return Promise.resolve(this.eulaUrl);
  }

  queryPrintServer(serverUrl: string): Promise<CupsPrintersList> {
    this.methodCalled('queryPrintServer', serverUrl);
    if (this.queryPrintServerResult !== PrintServerResult.NO_ERRORS) {
      return Promise.reject(this.queryPrintServerResult);
    }
    return Promise.resolve(this.printServerPrinters);
  }

  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus> {
    this.methodCalled('requestPrinterStatusUpdate', printerId);
    return Promise.resolve(this.printerStatusMap[printerId]!);
  }

  retrieveCupsPrinterPpd(printerId: string, printerName: string, eula: string):
      void {
    this.methodCalled('retrieveCupsPrinterPpd', [printerId, printerName, eula]);
  }

  getCupsPrinterPpdPath(): Promise<string> {
    this.methodCalled('getCupsPrinterPpdPath');
    return Promise.resolve(this.printerPpdPath);
  }

  openPrintManagementApp(): void {
    this.methodCalled('openPrintManagementApp');
  }

  openScanningApp(): void {
    this.methodCalled('openScanningApp');
  }

  setEulaUrl(eulaUrl: string): void {
    this.eulaUrl = eulaUrl;
  }

  setGetPrinterInfoResult(result: PrinterSetupResult): void {
    this.getPrinterInfoResult = result;
  }

  setQueryPrintServerResult(result: PrintServerResult): void {
    this.queryPrintServerResult = result;
  }

  setAddDiscoveredPrinterFailure(printer: CupsPrinterInfo): void {
    this.addDiscoveredFailedPrinter = printer;
  }

  setPpdReferenceResolved(resolved: boolean): void {
    this.printerInfo.ppdReferenceResolved = resolved;
  }

  addPrinterStatus(
      printerId: string, reason: PrinterStatusReason,
      severity: PrinterStatusSeverity): void {
    this.printerStatusMap[printerId] = {
      printerId,
      statusReasons: [
        {
          reason,
          severity,
        },
      ],
      timestamp: 0,
    };
  }
}
