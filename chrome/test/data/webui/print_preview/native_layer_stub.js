// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CapabilitiesResponse, Destination, LocalDestinationInfo, NativeInitialSettings, NativeLayer, PageLayoutInfo, PrinterType, ProvisionalDestinationInfo} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

import {getCddTemplate, getPdfPrinter} from './print_preview_test_utils.js';

/**
 * Test version of the native layer.
 * @implements {NativeLayer}
 */
export class NativeLayerStub extends TestBrowserProxy {
  constructor() {
    super([
      'dialogClose',
      'getInitialSettings',
      'getPrinters',
      'getPreview',
      'getPrinterCapabilities',
      'hidePreview',
      'print',
      'saveAppState',
      'showSystemDialog',
      'signIn',
    ]);

    /**
     * @private {?NativeInitialSettings} The initial settings
     *     to be used for the response to a |getInitialSettings| call.
     */
    this.initialSettings_ = null;

    /** @private {?Array<string>} Accounts to be sent on signIn(). */
    this.accounts_ = null;

    /**
     * @private {!Array<!LocalDestinationInfo>} Local
     *     destination list to be used for the response to |getPrinters|.
     */
    this.localDestinationInfos_ = [];

    /**
     * @private {!Array<!ProvisionalDestinationInfo>} Local
     *     destination list to be used for the response to |getPrinters|.
     */
    this.extensionDestinationInfos_ = [];

    /**
     * @private {!Map<string,
     *                !Promise<!CapabilitiesResponse>>}
     *     A map from destination IDs to the responses to be sent when
     *     |getPrinterCapabilities| is called for the ID.
     */
    this.localDestinationCapabilities_ = new Map();

    /** @private {?PromiseResolver} */
    this.multipleCapabilitiesPromise_ = null;

    /** @private {number} */
    this.multipleCapabilitiesCount_ = 0;

    /**
     * @private {string} The ID of a printer with a bad driver.
     */
    this.badPrinterId_ = '';

    /** @private {number} The number of total pages in the document. */
    this.pageCount_ = 1;

    /** @private {?PageLayoutInfo} Page layout information */
    this.pageLayoutInfo_ = null;
  }

  /** @param {number} pageCount The number of pages in the document. */
  setPageCount(pageCount) {
    this.pageCount_ = pageCount;
  }

  /** @override */
  dialogClose(isCancel) {
    this.methodCalled('dialogClose', isCancel);
  }

  /** @override */
  getInitialSettings() {
    this.methodCalled('getInitialSettings');
    return Promise.resolve(assert(this.initialSettings_));
  }

  /** @override */
  getPrinters(type) {
    this.methodCalled('getPrinters', type);
    if (type === PrinterType.LOCAL_PRINTER &&
        this.localDestinationInfos_.length > 0) {
      webUIListenerCallback(
          'printers-added', type, this.localDestinationInfos_);
    } else if (
        type === PrinterType.EXTENSION_PRINTER &&
        this.extensionDestinationInfos_.length > 0) {
      webUIListenerCallback(
          'printers-added', type, this.extensionDestinationInfos_);
    }
    return Promise.resolve();
  }

  /** @override */
  getPreview(printTicket) {
    this.methodCalled('getPreview', {printTicket: printTicket});
    const printTicketParsed = JSON.parse(printTicket);
    if (printTicketParsed.deviceName === this.badPrinterId_) {
      return Promise.reject('SETTINGS_INVALID');
    }
    const pageRanges = printTicketParsed.pageRange;
    const requestId = printTicketParsed.requestID;
    if (this.pageLayoutInfo_) {
      webUIListenerCallback('page-layout-ready', this.pageLayoutInfo_, false);
    }
    if (pageRanges.length === 0) {  // assume full length document, 1 page.
      webUIListenerCallback(
          'page-count-ready', this.pageCount_, requestId, 100);
      for (let i = 0; i < this.pageCount_; i++) {
        webUIListenerCallback('page-preview-ready', i, 0, requestId);
      }
    } else {
      const pages = pageRanges.reduce(function(soFar, range) {
        for (let page = range.from; page <= range.to; page++) {
          soFar.push(page);
        }
        return soFar;
      }, []);
      webUIListenerCallback(
          'page-count-ready', this.pageCount_, requestId, 100);
      pages.forEach(function(page) {
        webUIListenerCallback('page-preview-ready', page - 1, 0, requestId);
      });
    }
    return Promise.resolve(requestId);
  }

  /** @override */
  getPrinterCapabilities(printerId, type) {
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
    if (printerId === Destination.GooglePromotedId.SAVE_AS_PDF ||
        printerId === Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS) {
      return Promise.resolve(getPdfPrinter());
    }
    if (type !== PrinterType.LOCAL_PRINTER) {
      return Promise.reject();
    }
    return this.localDestinationCapabilities_.get(printerId) ||
        Promise.reject();
  }

  /** @override */
  print(printTicket) {
    this.methodCalled('print', printTicket);
    if (JSON.parse(printTicket).printerType === PrinterType.CLOUD_PRINTER) {
      return Promise.resolve('sample data');
    }
    return Promise.resolve();
  }

  /** @override */
  hidePreview() {
    this.methodCalled('hidePreview');
  }

  /** @override */
  showSystemDialog() {
    this.methodCalled('showSystemDialog');
  }

  /** @override */
  recordInHistogram() {}

  /** @override */
  saveAppState(appState) {
    this.methodCalled('saveAppState', appState);
  }

  /** @override */
  signIn() {
    this.methodCalled('signIn');
    const accounts = this.accounts_ || ['foo@chromium.org'];
    if (!this.accounts_) {
      accounts.push('bar@chromium.org');
    }
    if (accounts.length > 0) {
      webUIListenerCallback('user-accounts-updated', accounts);
    }
  }

  /** @override */
  cancelPendingPrintRequest() {}

  /** @override */
  managePrinters() {}

  /**
   * @param {!NativeInitialSettings} settings The settings
   *     to return as a response to |getInitialSettings|.
   */
  setInitialSettings(settings) {
    this.initialSettings_ = settings;
  }

  /**
   * @param {!Array<!LocalDestinationInfo>} localDestinations
   *     The local destinations to return as a response to |getPrinters|.
   */
  setLocalDestinations(localDestinations) {
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
   * @param {!Array<!ProvisionalDestinationInfo>}
   *     extensionDestinations The extension destinations to return as a
   *     response to |getPrinters|.
   */
  setExtensionDestinations(extensionDestinations) {
    this.extensionDestinationInfos_ = extensionDestinations;
  }

  /**
   * @param {!CapabilitiesResponse} response The
   *     response to send for the destination whose ID is in the response.
   * @param {boolean=} opt_reject Whether to reject the callback for this
   *     destination. Defaults to false (will resolve callback) if not
   *     provided.
   */
  setLocalDestinationCapabilities(response, opt_reject) {
    this.localDestinationCapabilities_.set(
        response.printer.deviceName,
        opt_reject ? Promise.reject() : Promise.resolve(response));
  }

  /**
   * @param {string} id The printer ID that should cause an
   *     SETTINGS_INVALID error in response to a preview request. Models a
   *     bad printer driver.
   */
  setInvalidPrinterId(id) {
    this.badPrinterId_ = id;
  }

  /** @param {!PageLayoutInfo} pageLayoutInfo */
  setPageLayoutInfo(pageLayoutInfo) {
    this.pageLayoutInfo_ = pageLayoutInfo;
  }

  /**
   * @param {number} count The number of capability requests to wait for.
   * @return {!Promise} Promise that resolves after |count| requests.
   */
  waitForMultipleCapabilities(count) {
    assert(this.multipleCapabilitiesPromise_ === null);
    this.multipleCapabilitiesCount_ = count;
    this.multipleCapabilitiesPromise_ = new PromiseResolver();
    return this.multipleCapabilitiesPromise_.promise;
  }
}
