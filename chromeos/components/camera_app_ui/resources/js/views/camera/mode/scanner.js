// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Filenamer} from '../../../models/file_namer.js';
import {ChromeHelper} from '../../../mojo/chrome_helper.js';
import {
  Facing,  // eslint-disable-line no-unused-vars
  MimeType,
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';

import {
  Photo,
  PhotoBaseFactory,
  PhotoHandler,  // eslint-disable-line no-unused-vars
  PhotoResult,   // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Provides external dependency functions used by photo mode and handles the
 * captured result photo.
 * @interface
 */
export class ScannerHandler {
  /**
   * Plays UI effect shutter effect blocking all UI operation.
   */
  playBlockingShutterEffect() {}

  /**
   * Clears UI effect shutter effect blocking all UI operation.
   */
  clearBlockingShutterEffect() {}

  /**
   * @param {!Blob} blob Jpeg Blob as scanned document.
   * @return {!Promise}
   */
  async setReviewDocument(blob) {}

  /**
   * @return {!Promise<?MimeType>}
   */
  async getDocumentReviewResult() {}

  /**
   * Handles the result document.
   * @param {!Blob} doc
   * @param {string} name Name of the document result to be saved as.
   * @return {!Promise}
   * @abstract
   */
  handleResultDocument(doc, name) {}
}

/**
 * @implements {PhotoHandler}
 */
class DocumentPhotoHandler {
  /**
   * @param {!ScannerHandler} handler
   */
  constructor(handler) {
    /**
     * @const {!ScannerHandler}
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  async handleResultPhoto({blob: rawBlob, resolution}) {
    const namer = new Filenamer();
    const helper = await ChromeHelper.getInstance();
    const corners = await helper.scanDocumentCorners(rawBlob);
    const jpegBlob =
        await helper.convertToDocument(rawBlob, corners, MimeType.JPEG);

    await this.handler_.setReviewDocument(jpegBlob);
    this.handler_.clearBlockingShutterEffect();
    const mimeType = await this.handler_.getDocumentReviewResult();
    switch (mimeType) {
      case null:
        return;
      case MimeType.JPEG:
        await this.handler_.handleResultDocument(
            jpegBlob, namer.newDocumentName(MimeType.JPEG));
        return;
      case MimeType.PDF:
        // TODO(b/190689433): Add code path handle pdf result.
        return;
    }
  }

  /**
   * @override
   */
  playShutterEffect() {
    this.handler_.playBlockingShutterEffect();
  }
}

/**
 * Photo mode capture controller.
 */
export class Scanner extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution
   * @param {!ScannerHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, new DocumentPhotoHandler(handler));

    /**
     * @const {!ScannerHandler}
     * @protected
     */
    this.scannerHandler_ = handler;
  }
}

/**
 * Factory for creating photo mode capture object.
 */
export class ScannerFactory extends PhotoBaseFactory {
  /**
   * @param {!ScannerHandler} handler
   */
  constructor(handler) {
    super();

    /**
     * @const {!ScannerHandler}
     * @protected
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  produce_() {
    return new Scanner(
        this.previewStream_,
        this.facing_,
        this.captureResolution_,
        this.handler_,
    );
  }
}
