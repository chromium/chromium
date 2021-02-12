// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {AppWindow} from './app_window.js';
// eslint-disable-next-line no-unused-vars
import {Intent} from './intent.js';
// eslint-disable-next-line no-unused-vars
import {PerfLogger} from './perf.js';
// eslint-disable-next-line no-unused-vars
import {TestingErrorCallback} from './type.js';

/**
 * Operations supported by foreground window.
 * @interface
 */
export class ForegroundOps {
  /**
   * Suspend foreground window.
   * @return {!Promise}
   * @abstract
   */
  async suspend() {}

  /**
   * Resume foreground window.
   * @abstract
   */
  resume() {}
}

/**
 * Operations supported by background window.
 * @interface
 */
export class BackgroundOps {
  /**
   * Sets the implementation of ForegroundOps from foreground window.
   * @param {!ForegroundOps} ops
   */
  bindForegroundOps(ops) {}

  /**
   * Sets the app window which is associated to the foreground window.
   * @param {?AppWindow} appWindow
   */
  bindAppWindow(appWindow) {}

  /**
   * Gets intent associate with CCA Window object.
   * @return {?Intent}
   * @abstract
   */
  getIntent() {}

  /**
   * Gets the perf logger associate with CCA Window object.
   * @return {!PerfLogger}
   * @abstract
   */
  getPerfLogger() {}

  /**
   * Gets callback for reporting error in testing run. Returns null in non
   * testing run.
   * @return {?TestingErrorCallback} callbacks
   * @abstract
   */
  getTestingErrorCallback() {}

  /**
   * Called by foreground window when it's active.
   * @abstract
   */
  notifyActivation() {}

  /**
   * Called by foreground window when it's suspended.
   * @abstract
   */
  notifySuspension() {}
}

/**
 * Creates a fake background ops.
 * @return {!BackgroundOps}
 */
export function createFakeBackgroundOps() {
  const perfLogger = new PerfLogger();
  const url = window.location.href;
  const intent = url.includes('intent') ? Intent.create(new URL(url)) : null;
  return /** @type {!BackgroundOps} */ ({
    bindForegroundOps: (ops) => {},
    bindAppWindow: (appWindow) => {},
    getIntent: () => intent,
    getPerfLogger: () => perfLogger,
    getTestingErrorCallback: () => null,
    notifyActivation: () => {},
    notifySuspension: () => {},
  });
}
