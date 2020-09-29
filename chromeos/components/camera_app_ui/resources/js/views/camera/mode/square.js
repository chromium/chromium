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
  PhotoHandler,  // eslint-disable-line no-unused-vars
} from './photo.js';

/**
 * Crops out maximum possible centered square from the image blob.
 * @param {!Blob} blob
 * @return {!Promise<!Blob>} Promise with result cropped square image.
 */
async function cropSquare(blob) {
  const img = await util.blobToImage(blob);
  const side = Math.min(img.width, img.height);
  const {canvas, ctx} = util.newDrawingCanvas({width: side, height: side});
  ctx.drawImage(
      img, Math.floor((img.width - side) / 2),
      Math.floor((img.height - side) / 2), side, side, 0, 0, side, side);
  const croppedBlob = await new Promise((resolve) => {
    canvas.toBlob(resolve, 'image/jpeg');
  });
  return croppedBlob;
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
    // Since the image blob after square cut will lose its EXIF including
    // orientation information. Corrects the orientation before the square
    // cut.
    result.blob = await new Promise(
        (resolve, reject) => util.orientPhoto(result.blob, resolve, reject));
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
