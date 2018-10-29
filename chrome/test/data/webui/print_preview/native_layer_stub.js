// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
   * Test version of the native layer.
   */
  class NativeLayerStub extends TestBrowserProxy {
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
        'setupPrinter',
        'showSystemDialog',
      ]);

      /**
       * @private {!print_preview.NativeInitialSettings} The initial settings
       *     to be used for the response to a |getInitialSettings| call.
       */
      this.initialSettings_ = null;

      /**
       * @private {!Array<!print_preview.LocalDestinationInfo>} Local
       *     destination list to be used for the response to |getPrinters|.
       */
      this.localDestinationInfos_ = [];

      /**
       * @private {!Array<!print_preview.ProvisionalDestinationInfo>} Local
       *     destination list to be used for the response to |getPrinters|.
       */
      this.extensionDestinationInfos_ = [];

      /**
       * @private {!Map<string,
       *                !Promise<!print_preview.CapabilitiesResponse>}
       *     A map from destination IDs to the responses to be sent when
       *     |getPrinterCapabilities| is called for the ID.
       */
      this.localDestinationCapabilities_ = new Map();

      /**
       * @private {!print_preview.PrinterSetupResponse} The response to be sent
       *     on a |setupPrinter| call.
       */
      this.setupPrinterResponse_ = null;

      /**
       * @private {boolean} Whether the printer setup request should be
       *     rejected.
       */
      this.shouldRejectPrinterSetup_ = false;

      /**
       * @private {string} The ID of a printer with a bad driver.
       */
      this.badPrinterId_ = '';

      /** @private {number} The number of total pages in the document. */
      this.pageCount_ = 1;
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
      return Promise.resolve(this.initialSettings_);
    }

    /** @override */
    getPrinters(type) {
      this.methodCalled('getPrinters', type);
      if (type == print_preview.PrinterType.LOCAL_PRINTER) {
        cr.webUIListenerCallback(
            'printers-added', type, this.localDestinationInfos_);
      } else if (
          type == print_preview.PrinterType.EXTENSION_PRINTER &&
          this.extensionDestinationInfos_.length > 0) {
        cr.webUIListenerCallback(
            'printers-added', type, this.extensionDestinationInfos_);
      }
      return Promise.resolve();
    }

    /** @override */
    getPreview(printTicket, pageCount) {
      this.methodCalled(
          'getPreview', {printTicket: printTicket, pageCount: pageCount});
      const printTicketParsed = JSON.parse(printTicket);
      if (printTicketParsed.deviceName == this.badPrinterId_)
        return Promise.reject('SETTINGS_INVALID');
      const pageRanges = printTicketParsed.pageRange;
      const requestId = printTicketParsed.requestID;
      if (pageRanges.length == 0) {  // assume full length document, 1 page.
        cr.webUIListenerCallback(
            'page-count-ready', this.pageCount_, requestId, 100);
        for (let i = 0; i < this.pageCount_; i++)
          cr.webUIListenerCallback('page-preview-ready', i, 0, requestId);
      } else {
        const pages = pageRanges.reduce(function(soFar, range) {
          for (let page = range.from; page <= range.to; page++) {
            soFar.push(page);
          }
          return soFar;
        }, []);
        cr.webUIListenerCallback(
            'page-count-ready', this.pageCount_, requestId, 100);
        pages.forEach(function(page) {
          cr.webUIListenerCallback(
              'page-preview-ready', page - 1, 0, requestId);
        });
      }
      return Promise.resolve(requestId);
    }

    /** @override */
    getPrivetPrinters() {
      this.methodCalled('getPrivetPrinters');
      return Promise.resolve(true);
    }

    /** @override */
    getPrinterCapabilities(printerId, type) {
      this.methodCalled(
          'getPrinterCapabilities',
          {destinationId: printerId, printerType: type});
      if (type != print_preview.PrinterType.LOCAL_PRINTER)
        return Promise.reject();
      return this.localDestinationCapabilities_.get(printerId);
    }

    /** @override */
    print(printTicket) {
      this.methodCalled('print', printTicket);
      return Promise.resolve();
    }

    /** @override */
    setupPrinter(printerId) {
      this.methodCalled('setupPrinter', printerId);
      return this.shouldRejectPrinterSetup_ ?
          Promise.reject(this.setupPrinterResponse_) :
          Promise.resolve(this.setupPrinterResponse_);
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
    recordAction() {}

    /** @override */
    recordInHistogram() {}

    /** @override */
    saveAppState(appState) {
      this.methodCalled('saveAppState', appState);
    }

    /**
     * @param {!print_preview.NativeInitialSettings} settings The settings
     *     to return as a response to |getInitialSettings|.
     */
    setInitialSettings(settings) {
      this.initialSettings_ = settings;
    }

    /**
     * @param {!Array<!print_preview.LocalDestinationInfo>} localDestinations
     *     The local destinations to return as a response to |getPrinters|.
     */
    setLocalDestinations(localDestinations) {
      this.localDestinationInfos_ = localDestinations;
    }

    /**
     * @param {!Array<!print_preview.ProvisionalDestinationInfo>}
     *     extensionDestinations The extension destinations to return as a
     *     response to |getPrinters|.
     */
    setExtensionDestinations(extensionDestinations) {
      this.extensionDestinationInfos_ = extensionDestinations;
    }

    /**
     * @param {!print_preview.CapabilitiesResponse} response The
     *     response to send for the destination whose ID is in the response.
     * @param {?boolean} opt_reject Whether to reject the callback for this
     *     destination. Defaults to false (will resolve callback) if not
     *     provided.
     */
    setLocalDestinationCapabilities(response, opt_reject) {
      this.localDestinationCapabilities_.set(
          response.printer.deviceName,
          opt_reject ? Promise.reject() : Promise.resolve(response));
    }

    /**
     * @param {!print_preview.PrinterSetupResponse} The response to send when
     *     |setupPrinter| is called.
     * @param {?boolean} opt_reject Whether printSetup requests should be
     *     rejected. Defaults to false (will resolve callback) if not provided.
     */
    setSetupPrinterResponse(response, opt_reject) {
      this.shouldRejectPrinterSetup_ = opt_reject || false;
      this.setupPrinterResponse_ = response;
    }

    /**
     * @param {string} bad_id The printer ID that should cause an
     *     SETTINGS_INVALID error in response to a preview request. Models a
     *     bad printer driver.
     */
    setInvalidPrinterId(id) {
      this.badPrinterId_ = id;
    }
  }

  return {
    NativeLayerStub: NativeLayerStub,
  };
});
