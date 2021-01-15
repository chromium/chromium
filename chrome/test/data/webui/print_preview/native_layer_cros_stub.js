// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayerCros, NativeLayerCrosImpl, PrinterSetupResponse, PrintServer, PrintServersConfig} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @return {!NativeLayerCrosStub} */
export function setNativeLayerCrosInstance() {
  const instance = new NativeLayerCrosStub();
  NativeLayerCrosImpl.instance_ = instance;
  return instance;
}

/**
 * Test version of the Chrome OS native layer.
 * @implements {NativeLayerCros}
 */
export class NativeLayerCrosStub extends TestBrowserProxy {
  constructor() {
    super([
      'getEulaUrl',
      'requestPrinterStatusUpdate',
      'setupPrinter',
      'choosePrintServers',
      'getPrintServersConfig',
    ]);

    /**
     * @private {?PrinterSetupResponse} The response to be sent
     *     on a |setupPrinter| call.
     */
    this.setupPrinterResponse_ = null;

    /**
     * @private {boolean} Whether the printer setup request should be
     *     rejected.
     */
    this.shouldRejectPrinterSetup_ = false;

    /** @private {string} license The PPD license of a destination. */
    this.eulaUrl_ = '';

    /**
     * @private {!Map<string, !Object>}
     * A map from printerId to PrinterStatus. Defining the value parameter as
     * Object instead of PrinterStatus because the PrinterStatus type is CrOS
     * specific, and this class is used by tests on all platforms.
     */
    this.printerStatusMap_ = new Map();

    /** @private {?PromiseResolver} */
    this.multiplePrinterStatusRequestsPromise_ = null;

    /** @private {number} */
    this.multiplePrinterStatusRequestsCount_ = 0;

    /** @private {!PrintServersConfig} */
    this.printServersConfig_ = {
      printServers: [],
      isSingleServerFetchingMode: false
    };
  }

  /** @override */
  getEulaUrl(destinationId) {
    this.methodCalled('getEulaUrl', {destinationId: destinationId});

    return Promise.resolve(this.eulaUrl_);
  }

  /** @override */
  setupPrinter(printerId) {
    this.methodCalled('setupPrinter', printerId);
    return this.shouldRejectPrinterSetup_ ?
        Promise.reject(assert(this.setupPrinterResponse_)) :
        Promise.resolve(assert(this.setupPrinterResponse_));
  }

  /** @override */
  getAccessToken() {}

  /** @override */
  grantExtensionPrinterAccess() {}

  /**
   * @param {!PrinterSetupResponse} response The response to send when
   *     |setupPrinter| is called.
   * @param {boolean=} opt_reject Whether printSetup requests should be
   *     rejected. Defaults to false (will resolve callback) if not provided.
   */
  setSetupPrinterResponse(response, opt_reject) {
    this.shouldRejectPrinterSetup_ = opt_reject || false;
    this.setupPrinterResponse_ = response;
  }

  /** @param {string} eulaUrl The eulaUrl of the PPD. */
  setEulaUrl(eulaUrl) {
    this.eulaUrl_ = eulaUrl;
  }

  /**
   * Sends a request to the printer with id |printerId| for its current status.
   * @param {string} printerId
   * @return {!Promise} Promise that resolves returns a printer status.
   * @override
   */
  requestPrinterStatusUpdate(printerId) {
    this.methodCalled('requestPrinterStatusUpdate');
    if (this.multiplePrinterStatusRequestsPromise_) {
      this.multiplePrinterStatusRequestsCount_--;
      if (this.multiplePrinterStatusRequestsCount_ === 0) {
        this.multiplePrinterStatusRequestsPromise_.resolve();
        this.multiplePrinterStatusRequestsPromise_ = null;
      }
    }

    return Promise.resolve(this.printerStatusMap_.get(printerId) || {});
  }

  /**
   * @param {string} printerId
   * @param {!Object} printerStatus
   */
  addPrinterStatusToMap(printerId, printerStatus) {
    this.printerStatusMap_.set(printerId, printerStatus);
  }

  /**
   * @param {number} count The number of printer status requests to wait for.
   * @return {!Promise} Promise that resolves after |count| requests.
   */
  waitForMultiplePrinterStatusRequests(count) {
    assert(this.multiplePrinterStatusRequestsPromise_ === null);
    this.multiplePrinterStatusRequestsCount_ = count;
    this.multiplePrinterStatusRequestsPromise_ = new PromiseResolver();
    return this.multiplePrinterStatusRequestsPromise_.promise;
  }

  /** @override */
  recordPrinterStatusHistogram(statusReason, didUserAttemptPrint) {}

  /** @override */
  choosePrintServers(printServerIds) {
    this.methodCalled('choosePrintServers', printServerIds);
  }

  /** @override */
  getPrintServersConfig() {
    this.methodCalled('getPrintServersConfig');
    return Promise.resolve(this.printServersConfig_);
  }

  /** @param {!PrintServersConfig} printServersConfig */
  setPrintServersConfig(printServersConfig) {
    this.printServersConfig_ = printServersConfig;
  }
}
