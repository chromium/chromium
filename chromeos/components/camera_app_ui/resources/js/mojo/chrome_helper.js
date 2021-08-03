// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../chrome_util.js';
import {reportError} from '../error.js';
import {
  ErrorLevel,
  ErrorType,
  MimeType,  // eslint-disable-line no-unused-vars
} from '../type.js';
import {windowController} from '../window_controller.js';

import {wrapEndpoint} from './util.js';

/**
 * The singleton instance of ChromeHelper. Initialized by the first
 * invocation of getInstance().
 * @type {?ChromeHelper}
 */
let instance = null;

/**
 * Forces casting type from Uint8Array to !Array<number>.
 * @param {!Uint8Array} data
 * @return {!Array<number>}
 * @suppress {checkTypes}
 * @private
 */
function castToNumberArray(data) {
  return data;
}

/**
 * Communicates with Chrome.
 */
export class ChromeHelper {
  /**
   * @public
   */
  constructor() {
    /**
     * An interface remote that is used to communicate with Chrome.
     * @type {!chromeosCamera.mojom.CameraAppHelperRemote}
     */
    this.remote_ =
        wrapEndpoint(chromeosCamera.mojom.CameraAppHelper.getRemote());
  }

  /**
   * Starts tablet mode monitor monitoring tablet mode state of device.
   * @param {function(boolean): void} onChange Callback called each time when
   *     tablet mode state of device changes with boolean parameter indecating
   *     whether device is entering tablet mode.
   * @return {!Promise<boolean>} Resolved to initial state of whether device is
   *     is in tablet mode.
   */
  async initTabletModeMonitor(onChange) {
    const monitorCallbackRouter = wrapEndpoint(
        new chromeosCamera.mojom.TabletModeMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {isTabletMode} = await this.remote_.setTabletMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return isTabletMode;
  }

  /**
   * Starts monitor monitoring system screen state of device.
   * @param {function(!chromeosCamera.mojom.ScreenState): void} onChange
   *     Callback called each time when device screen state changes with
   *     parameter of newly changed value.
   * @return {!Promise<!chromeosCamera.mojom.ScreenState>} Resolved to initial
   *     system screen state.
   */
  async initScreenStateMonitor(onChange) {
    const monitorCallbackRouter = wrapEndpoint(
        new chromeosCamera.mojom.ScreenStateMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {initialState} = await this.remote_.setScreenStateMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return initialState;
  }

  /**
   * Starts monitor monitoring the existence of external screens.
   * @param {function(boolean): void} onChange Callback called when the
   *     existence of external screens changes.
   * @return {!Promise<boolean>} Resolved to the initial state.
   */
  async initExternalScreenMonitor(onChange) {
    const monitorCallbackRouter = wrapEndpoint(
        new chromeosCamera.mojom.ExternalScreenMonitorCallbackRouter());
    monitorCallbackRouter.update.addListener(onChange);

    const {hasExternalScreen} = await this.remote_.setExternalScreenMonitor(
        monitorCallbackRouter.$.bindNewPipeAndPassRemote());
    return hasExternalScreen;
  }

  /**
   * Checks if the device is under tablet mode currently.
   * @return {!Promise<boolean>}
   */
  async isTabletMode() {
    const {isTabletMode} = await this.remote_.isTabletMode();
    return isTabletMode;
  }

  /**
   * Starts camera usage monitor.
   * @param {function(): !Promise} exploitUsage
   * @param {function(): !Promise} releaseUsage
   * @return {!Promise}
   */
  async initCameraUsageMonitor(exploitUsage, releaseUsage) {
    const usageCallbackRouter = wrapEndpoint(
        new chromeosCamera.mojom.CameraUsageOwnershipMonitorCallbackRouter());

    usageCallbackRouter.onCameraUsageOwnershipChanged.addListener(
        async (hasUsage) => {
          if (hasUsage) {
            await exploitUsage();
          } else {
            await releaseUsage();
          }
        });

    await this.remote_.setCameraUsageMonitor(
        usageCallbackRouter.$.bindNewPipeAndPassRemote());

    let {controller} = await this.remote_.getWindowStateController();
    controller = wrapEndpoint(controller);
    await windowController.bind(controller);
  }

  /**
   * Triggers the begin of event tracing in Chrome.
   * @param {string} event Name of the event.
   */
  startTracing(event) {
    this.remote_.startPerfEventTrace(event);
  }

  /**
   * Triggers the end of event tracing in Chrome.
   * @param {string} event Name of the event.
   */
  stopTracing(event) {
    this.remote_.stopPerfEventTrace(event);
  }

  /**
   * Opens the file in Downloads folder by its |name| in gallery.
   * @param {string} name Name of the target file.
   */
  openFileInGallery(name) {
    this.remote_.openFileInGallery(name);
  }

  /**
   * Opens the chrome feedback dialog.
   * @param {string} placeholder The text of the placeholder in the description
   *     field.
   */
  openFeedbackDialog(placeholder) {
    this.remote_.openFeedbackDialog(placeholder);
  }

  /**
   * Checks return value from |handleCameraResult|.
   * @param {string} caller Caller identifier.
   * @param {!Promise<{isSuccess: boolean}>} value
   * @return {!Promise}
   */
  async checkReturn_(caller, value) {
    const {isSuccess} = await value;
    if (!isSuccess) {
      reportError(
          ErrorType.HANDLE_CAMERA_RESULT_FAILURE, ErrorLevel.ERROR,
          new Error(`Return not isSuccess from calling intent ${caller}.`));
    }
  }

  /**
   * Notifies ARC++ to finish the intent.
   * @param {number} intentId Intent id of the intent to be finished.
   * @return {!Promise}
   */
  async finish(intentId) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.FINISH, []);
    await this.checkReturn_('finish()', ret);
  }

  /**
   * Notifies ARC++ to append data to intent result.
   * @param {number} intentId Intent id of the intent to be appended data to.
   * @param {!Uint8Array} data The data to be appended to intent result.
   * @return {!Promise}
   */
  async appendData(intentId, data) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.APPEND_DATA,
        castToNumberArray(data));
    await this.checkReturn_('appendData()', ret);
  }

  /**
   * Notifies ARC++ to clear appended intent result data.
   * @param {number} intentId Intent id of the intent to be cleared its result.
   * @return {!Promise}
   */
  async clearData(intentId) {
    const ret = this.remote_.handleCameraResult(
        intentId, arc.mojom.CameraIntentAction.CLEAR_DATA, []);
    await this.checkReturn_('clearData()', ret);
  }

  /**
   * Checks if the logging consent option is enabled.
   * @return {!Promise<boolean>}
   */
  async isMetricsAndCrashReportingEnabled() {
    const {isEnabled} = await this.remote_.isMetricsAndCrashReportingEnabled();
    return isEnabled;
  }

  /**
   * Sends the broadcast to ARC to notify the new photo/video is captured.
   * @param {{isVideo: boolean, name: string}} info
   * @return {!Promise}
   */
  async sendNewCaptureBroadcast({isVideo, name}) {
    this.remote_.sendNewCaptureBroadcast(isVideo, name);
  }

  /**
   * Monitors for the file deletion of the file given by its |name| and triggers
   * |callback| when the file is deleted. Note that a previous monitor request
   * will be canceled once another monitor request is sent.
   * @param {string} name The name of the file to monitor.
   * @param {function(): void} callback Function to trigger when deletion.
   * @return {!Promise} Resolved when the file is deleted or the current monitor
   *     is canceled by future monitor call.
   * @throws {!Error} When error occurs during monitor.
   */
  async monitorFileDeletion(name, callback) {
    const {result} = await this.remote_.monitorFileDeletion(name);
    switch (result) {
      case chromeosCamera.mojom.FileMonitorResult.DELETED:
        callback();
        return;
      case chromeosCamera.mojom.FileMonitorResult.CANCELED:
        // Do nothing if it is canceled by another monitor call.
        return;
      case chromeosCamera.mojom.FileMonitorResult.ERROR:
        throw new Error('Error happens when monitoring file deletion');
    }
  }

  /**
   * Returns true if the document mode is supported on the device.
   * @return {!Promise<boolean>}
   */
  async isDocumentModeSupported() {
    const {isSupported} = await this.remote_.isDocumentModeSupported();
    return isSupported;
  }

  /**
   * Scans the blob data and returns the detected document corners.
   * @param {!Blob} blob
   * @return {!Promise<!Array<!gfx.mojom.PointF>>}
   */
  async scanDocumentCorners(blob) {
    const buffer = new Uint8Array(await blob.arrayBuffer());

    const {corners} =
        await this.remote_.scanDocumentCorners(castToNumberArray(buffer));
    return corners;
  }

  /**
   * Converts the blob to document given by its |blob| data, |resolution| and
   * target |corners| to crop. The output will be converted according to given
   * |mimeType|.
   * @param {!Blob} blob
   * @param {!Array<!gfx.mojom.PointF>} corners
   * @param {!MimeType} mimeType
   * @return {!Promise<!Blob>}
   */
  async convertToDocument(blob, corners, mimeType) {
    assert(corners.length === 4, 'Unexpected amount of corners');
    const buffer = new Uint8Array(await blob.arrayBuffer());
    let outputFormat;
    if (mimeType === MimeType.JPEG) {
      outputFormat = chromeosCamera.mojom.DocumentOutputFormat.JPEG;
    } else if (mimeType === MimeType.PDF) {
      outputFormat = chromeosCamera.mojom.DocumentOutputFormat.PDF;
    } else {
      throw new Error(`Output mimetype unsupported: ${mimeType}`);
    }

    const {docData} = await this.remote_.convertToDocument(
        castToNumberArray(buffer), corners, outputFormat);
    return new Blob([new Uint8Array(docData)], {type: mimeType});
  }

  /**
   * Converts given |jpegData| to PDF format.
   * @param {!Blob} jpegBlob Blob in JPEG format.
   * @return {!Promise<!Blob>} Blob in PDF format.
   */
  async convertToPdf(jpegBlob) {
    const buffer = new Uint8Array(await jpegBlob.arrayBuffer());
    const {pdfData} =
        await this.remote_.convertToPdf(castToNumberArray(buffer));
    return new Blob([new Uint8Array(pdfData)], {type: MimeType.PDF});
  }

  /**
   * Creates a new instance of ChromeHelper if it is not set. Returns the
   *     exist instance.
   * @return {!ChromeHelper} The singleton instance.
   */
  static getInstance() {
    if (instance === null) {
      instance = new ChromeHelper();
    }
    return instance;
  }
}
