// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Facing,      // eslint-disable-line no-unused-vars
  Resolution,  // eslint-disable-line no-unused-vars
} from '../../../type.js';
import * as util from '../../../util.js';

import {
  Photo,
  PhotoFactory,
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Crops out maximum possible centered square from the image blob.
 * @param {!Blob} blob
 * @return {!Promise<!Blob>} Promise with result cropped square image.
 */
async function cropSquare(blob) {
  const img = await util.blobToImage(blob);
  try {
    const side = Math.min(img.width, img.height);
    const {canvas, ctx} = util.newDrawingCanvas({width: side, height: side});
    ctx.drawImage(
        img, Math.floor((img.width - side) / 2),
        Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
    const croppedBlob = await new Promise((resolve) => {
      // TODO(b/174190121): Patch important exif entries from input blob to
      // result blob.
      canvas.toBlob(resolve, 'image/jpeg');
    });
    return croppedBlob;
  } finally {
    URL.revokeObjectURL(img.src);
  }
}

/**
 * Cuts the returned photo into square and passed to underlying PhotoHandler.
 * @implements {PhotoHandler}
 */
class SquarePhotoHandler {
  /**
   * @param {!PhotoHandler} handler
   */
  constructor(handler) {
    /**
     * @const {!PhotoHandler}
     */
    this.handler_ = handler;
  }

  /**
   * @override
   */
  async handleResultPhoto(result, ...args) {
    result.blob = await cropSquare(result.blob);
    await this.handler_.handleResultPhoto(result, ...args);
  }

  /**
   * @override
   */
  playShutterEffect() {
    this.handler_.playShutterEffect();
  }
}

/**
 * Square mode capture controller.
 */
export class Square extends Photo {
  /**
   * @param {!MediaStream} stream
   * @param {!Facing} facing
   * @param {?Resolution} captureResolution
   * @param {!PhotoHandler} handler
   */
  constructor(stream, facing, captureResolution, handler) {
    super(stream, facing, captureResolution, new SquarePhotoHandler(handler));
  }
}

/**
 * Factory for creating square mode capture object.
 */
export class SquareFactory extends PhotoFactory {
  /**
   * @override
   */
  produce_() {
    return new Square(
        this.previewStream_, this.facing_, this.captureResolution_,
        this.handler_);
  }
}
