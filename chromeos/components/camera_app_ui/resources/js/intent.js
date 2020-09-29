// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as metrics from './metrics.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {Mode} from './type.js';

/**
 * Thrown when fails to parse intent url.
 */
export class ParseError extends Error {
  /**
   * @param {!URL} url Intent url.
   */
  constructor(url) {
    super(`Failed to parse intent url ${url}`);
    /**
     * @const {!URL} url
     * @private
     */
    this.url_ = url;

    this.name = this.constructor.name;
  }
}

/**
 * Intent from ARC++.
 */
export class Intent {
  /**
   * @param {!URL} url
   * @param {number} intentId
   * @param {!Mode} mode
   * @param {boolean} shouldHandleResult
   * @param {boolean} shouldDownScale
   * @param {boolean} isSecure
   * @private
   */
  constructor(
      url, intentId, mode, shouldHandleResult, shouldDownScale, isSecure) {
    /**
     * @const {!URL}
     */
    this.url = url;

    /**
     * @const {number}
     */
    this.intentId = intentId;

    /**
     * Capture mode of intent.
     * @const {!Mode}
     */
    this.mode = mode;

    /**
     * Whether the intent should return with the captured result.
     * @const {boolean}
     */
    this.shouldHandleResult = shouldHandleResult;

    /**
     * Whether the captured image should be down-scaled.
     * @const {boolean}
     */
    this.shouldDownScale = shouldDownScale;

    /**
     * If the intent is launched when the device is under secure mode.
     * @const {boolean}
     */
    this.isSecure = isSecure;

    /**
     * Flag for avoiding intent being resolved by foreground and background
     * twice.
     * @type {boolean}
     * @private
     */
    this.done_ = false;
  }

  /**
   * @return {!ChromeHelper}
   * @private
   */
  get chromeHelper_() {
    return ChromeHelper.getInstance();
  }

  /**
   * Whether intent has been finished or canceled.
   * @return {boolean}
   */
  get done() {
    return this.done_;
  }

  /**
   * Notifies ARC++ to finish the intent.
   * @return {!Promise}
   */
  async finish() {
    if (this.done) {
      return;
    }
    this.done_ = true;
    await this.chromeHelper_.finish(this.intentId);
    this.logResult(metrics.IntentResultType.CONFIRMED);
  }

  /**
   * Notifies ARC++ to cancel the intent.
   * @return {!Promise}
   */
  async cancel() {
    if (this.done) {
      return;
    }
    this.done_ = true;
    await this.chromeHelper_.cancel(this.intentId);
    this.logResult(metrics.IntentResultType.CANCELED);
  }

  /**
   * Notifies ARC++ to append data to the intent result.
   * @param {!Uint8Array} data The data to be appended to intent result.
   * @return {!Promise}
   */
  async appendData(data) {
    if (this.done) {
      return;
    }
    await this.chromeHelper_.appendData(this.intentId, data);
  }

  /**
   * Notifies ARC++ to clear appended intent result data.
   * @return {!Promise}
   */
  async clearData() {
    if (this.done) {
      return;
    }
    await this.chromeHelper_.clearData(this.intentId);
  }

  /**
   * Logs the intent result to metrics.
   * @param {!metrics.IntentResultType} result
   */
  logResult(result) {
    metrics.sendIntentEvent({
      mode: this.mode,
      result,
      shouldHandleResult: this.shouldHandleResult,
      shouldDownScale: this.shouldDownScale,
      isSecure: this.isSecure,
    });
  }

  /**
   * @param {!URL} url Url passed along with app launch event.
   * @return {!Intent} Created intent object. Returns null if input is not a
   *     valid intent url.
   * @throws {!ParseError}
   */
  static create(url) {
    const params = url.searchParams;
    const getBool = (key) => params.get(key) === '1';
    let param = params.get('intentId');
    if (param === null) {
      throw new ParseError(url);
    }
    const intentId = parseInt(param, 10);

    param = params.get('mode');
    if (param === null || !Object.values(Mode).includes(param)) {
      throw new ParseError(url);
    }
    const mode = /** @type {!Mode} */ (param);

    return new Intent(
        url, intentId, mode, getBool('shouldHandleResult'),
        getBool('shouldDownScale'), getBool('isSecure'));
  }
}
