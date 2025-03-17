// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CapabilitiesResponse, ExtensionDestinationInfo, LocalDestinationInfo, NativeInitialSettings, NativeLayer, PageLayoutInfo} from 'chrome://print/print_preview.js';
import {GooglePromotedDestinationId, PrinterType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {getCddTemplate, getPdfPrinter} from './print_preview_test_utils.js';

/**
 * Test version of the native layer.
 */
export class NativeLayerStub extends TestBrowserProxy implements NativeLayer {
  /**
   * The initial settings to be used for the response to a |getInitialSettings|
   * call.
   */
  private initialSettings_: NativeInitialSettings|null = null;

  /**
   * Local destination list to be used for the response to |getPrinters|.
   */
  private localDestinationInfos_: LocalDestinationInfo[] = [];

  /**
   * Extension destination lists to be used for the response to |getPrinters|.
   */
  private extensionDestinationInfos_: ExtensionDestinationInfo[][] = [];

  /**
   *     A map from destination IDs to the responses to be sent when
   *     |getPrinterCapabilities| is called for the ID.
   */
  private localDestinationCapabilities_:
      Map<string, Promise<CapabilitiesResponse>> = new Map();

  private multipleCapabilitiesPromise_: PromiseResolver<void>|null = null;

  private multipleCapabilitiesCount_: number = 0;

  private multipleGetPrintersPromise_: PromiseResolver<void>|null = null;

  private multipleGetPrintersCount_: number = 0;

  /** The ID of a printer with a simulated bad driver. */
  private badPrinterId_: string = '';

  /** The number of total pages in the document. */
  private pageCount_: number = 1;

  private pageLayoutInfo_: PageLayoutInfo|null = null;

  /**
   * Rejects the promise for getPrinters() to simulate getting no response or a
   * a slow response from the backend.
   */
  private simulateNoResponseForGetPrinters_: boolean = false;

  constructor() {
    super([
      'dialogClose',
      'doPrint',
      'getInitialSettings',
      'getPrinters',
      'getPreview',
      'getPrinterCapabilities',
      'hidePreview',
      'managePrinters',
      'recordInHistogram',
      'saveAppState',
      'showSystemDialog',
    ]);
  }

  setPageCount(pageCount: number) {
    this.pageCount_ = pageCount;
  }

  dialogClose(isCancel: boolean) {
    this.methodCalled('dialogClose', isCancel);
  }

  getInitialSettings() {
    this.methodCalled('getInitialSettings');
    assert(this.initialSettings_);
    return Promise.resolve(this.initialSettings_);
  }

  getPrinters(type: PrinterType) {
    if (this.simulateNoResponseForGetPrinters_) {
      return Promise.reject();
    }

    this.methodCalled('getPrinters', type);
    if (this.multipleGetPrintersPromise_) {
      this.multipleGetPrintersCount_--;
      if (this.multipleGetPrintersCount_ === 0) {
        this.multipleGetPrintersPromise_.resolve();
        this.multipleGetPrintersPromise_ = null;
      }
    }

    if (type === PrinterType.LOCAL_PRINTER &&
        this.localDestinationInfos_.length > 0) {
      webUIListenerCallback(
          'printers-added', type, this.localDestinationInfos_);
    } else if (
        type === PrinterType.EXTENSION_PRINTER &&
        this.extensionDestinationInfos_.length > 0) {
      this.extensionDestinationInfos_.forEach(infoList => {
        webUIListenerCallback('printers-added', type, infoList);
      });
    }
    return Promise.resolve();
  }

  getPreview(printTicket: string) {
    this.methodCalled('getPreview', {printTicket: printTicket});
    const printTicketParsed = JSON.parse(printTicket);
    if (printTicketParsed.deviceName === this.badPrinterId_) {
      return Promise.reject('SETTINGS_INVALID');
    }
    const pageRanges = printTicketParsed.pageRange;
    const requestId = printTicketParsed.requestID;
    if (this.pageLayoutInfo_) {
      webUIListenerCallback(
          'page-layout-ready', this.pageLayoutInfo_, false, false);
    }
    if (pageRanges.length === 0) {  // assume full length document, 1 page.
      webUIListenerCallback(
          'page-count-ready', this.pageCount_, requestId, 100);
      for (let i = 0; i < this.pageCount_; i++) {
        webUIListenerCallback('page-preview-ready', i, 0, requestId);
      }
    } else {
      const pages = pageRanges.reduce(
          function(soFar: number[], range: {from: number, to: number}) {
            for (let page = range.from; page <= range.to; page++) {
              soFar.push(page);
            }
            return soFar;
          },
          []);
      webUIListenerCallback(
          'page-count-ready', this.pageCount_, requestId, 100);
      pages.forEach(function(page: number) {
        webUIListenerCallback('page-preview-ready', page - 1, 0, requestId);
      });
    }
    return Promise.resolve(requestId);
  }

  getPrinterCapabilities(printerId: string, type: PrinterType) {
    this.methodCalled(
        'getPrinterCapabilities',
        {destinationId: printerId, printerType: type});
    if (this.multipleCapabilitiesPromise_) {
      this.multipleCapabilitiesCount_--;
      if (this.multipleCapabilitiesCount_ === 0) {
        this.multipleCapabilitiesPromise_.resolve();
        this.multipleCapabilitiesPromise_ = null;
      }
    }
    if (printerId === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return Promise.resolve(getPdfPrinter());
    }
    // <if expr="is_chromeos">
    if (printerId === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return Promise.resolve(getPdfPrinter());
    }
    // </if>
    if (type !== PrinterType.LOCAL_PRINTER) {
      return Promise.reject();
    }
    return this.localDestinationCapabilities_.get(printerId) ||
        Promise.reject();
  }

  doPrint(printTicket: string) {
    this.methodCalled('doPrint', printTicket);
    return Promise.resolve(undefined);
  }

  hidePreview() {
    this.methodCalled('hidePreview');
  }

  showSystemDialog() {
    this.methodCalled('showSystemDialog');
  }

  recordInHistogram(histogram: string, bucket: number) {
    this.methodCalled('recordInHistogram', histogram, bucket);
  }

  recordBooleanHistogram() {}

  saveAppState(appState: string) {
    this.methodCalled('saveAppState', appState);
  }

  cancelPendingPrintRequest() {}

  managePrinters() {
    this.methodCalled('managePrinters');
  }

  /**
   * settings The settings to return as a response to |getInitialSettings|.
   */
  setInitialSettings(settings: NativeInitialSettings) {
    this.initialSettings_ = settings;
  }

  /**
   * @param localDestinations The local destinations to return as a response to
   *     |getPrinters|.
   */
  setLocalDestinations(localDestinations: LocalDestinationInfo[]) {
    this.localDestinationInfos_ = localDestinations;
    this.localDestinationCapabilities_ = new Map();
    this.localDestinationInfos_.forEach(info => {
      this.setLocalDestinationCapabilities({
        printer: info,
        capabilities:
            getCddTemplate(info.deviceName, info.printerName).capabilities,
      });
    });
  }

  /**
   * @param extensionDestinations The extension destinations to return as a
   *     response to |getPrinters|.
   */
  setExtensionDestinations(extensionDestinations:
                               ExtensionDestinationInfo[][]) {
    this.extensionDestinationInfos_ = extensionDestinations;
  }

  /**
   * @param response The response to send for the destination whose ID is in the
   *     response.
   * @param reject Whether to reject the callback for this destination.
   *     Defaults to false (will resolve callback) if not provided.
   */
  setLocalDestinationCapabilities(
      response: CapabilitiesResponse, reject?: boolean) {
    this.localDestinationCapabilities_.set(
        response.printer!.deviceName,
        reject ? Promise.reject() : Promise.resolve(response));
  }

  /**
   * @param id The printer ID that should cause an SETTINGS_INVALID error in
   *     response to a preview request. Models a bad printer driver.
   */
  setInvalidPrinterId(id: string) {
    this.badPrinterId_ = id;
  }

  setPageLayoutInfo(pageLayoutInfo: PageLayoutInfo) {
    this.pageLayoutInfo_ = pageLayoutInfo;
  }

  /**
   * @param count The number of capability requests to wait for.
   * @return Promise that resolves after |count| requests.
   */
  waitForMultipleCapabilities(count: number): Promise<void> {
    assert(this.multipleCapabilitiesPromise_ === null);
    this.multipleCapabilitiesCount_ = count;
    this.multipleCapabilitiesPromise_ = new PromiseResolver();
    return this.multipleCapabilitiesPromise_.promise;
  }

  /**
   * @param count The number of getPrinters requests to wait for.
   * @return Promise that resolves after |count| requests.
   */
  waitForGetPrinters(count: number): Promise<void> {
    assert(this.multipleGetPrintersPromise_ === null);
    this.multipleGetPrintersCount_ = count;
    this.multipleGetPrintersPromise_ = new PromiseResolver();
    return this.multipleGetPrintersPromise_.promise;
  }

  setSimulateNoResponseForGetPrinters(simulateNoResponseForGetPrinters:
                                          boolean) {
    this.simulateNoResponseForGetPrinters_ = simulateNoResponseForGetPrinters;
  }
}
