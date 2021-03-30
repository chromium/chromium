// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from './dom.js';
import {reportError} from './error.js';
import {
  ErrorLevel,
  ErrorType,
} from './type.js';
import {newDrawingCanvas} from './util.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Converts the element to a jpeg blob by drawing it on a canvas.
 * @param {!CanvasImageSource} element Source element.
 * @param {number} width Canvas width.
 * @param {number} height Canvas height.
 * @return {!Promise<!Blob>} Converted jpeg blob.
 */
async function elementToJpegBlob(element, width, height) {
  const {canvas, ctx} = newDrawingCanvas({width, height});
  ctx.drawImage(element, 0, 0, width, height);

  /**
   * @type {!Uint8ClampedArray} A one-dimensional pixels array in RGBA order.
   */
  const data = ctx.getImageData(0, 0, width, height).data;
  if (data.every((byte) => byte === 0)) {
    reportError(
        ErrorType.BROKEN_THUMBNAIL,
        ErrorLevel.ERROR,
        new Error('The thumbnail is empty'),
    );
    // Do not throw an error here. A black thumbnail is still better than no
    // thumbnail to let user open the corresponding picutre in gallery.
  }

  return new Promise((resolve) => {
    canvas.toBlob(resolve, 'image/jpeg');
  });
}

/**
 * Loads the blob into a <video> element.
 * @param {!Blob} blob
 * @return {!Promise<!HTMLVideoElement>}
 */
async function loadVideoBlob(blob) {
  const el = dom.create('video', HTMLVideoElement);

  try {
    await new Promise((resolve, reject) => {
      el.addEventListener('error', () => {
        reject(new Error(`Failed to load video: ${el.error.message}`));
      });
      const gotFrame = new WaitableEvent();
      el.requestVideoFrameCallback(() => gotFrame.signal());
      el.preload = 'auto';
      el.src = URL.createObjectURL(blob);
      Promise.allSettled([el.play(), gotFrame.timedWait(300)]).then(() => {
        el.pause();
        resolve();
      });
    });
  } finally {
    URL.revokeObjectURL(el.src);
  }

  return el;
}

/**
 * Loads the blob into an <img> element.
 * @param {!Blob} blob
 * @return {!Promise<!HTMLImageElement>}
 */
async function loadImageBlob(blob) {
  const el = new Image();
  try {
    await new Promise((resolve, reject) => {
      el.addEventListener('error', () => {
        reject(new Error('Failed to load image'));
      });
      el.addEventListener('load', () => {
        resolve();
      });
      el.src = URL.createObjectURL(blob);
    });
  } finally {
    URL.revokeObjectURL(el.src);
  }
  return el;
}

/**
 * Creates a thumbnail of video by scaling the first frame to the target size.
 * @param {!Blob} blob Blob of video to be scaled.
 * @param {number} width Target width.
 * @param {number=} height Target height. Preserve the aspect ratio if not set.
 * @return {!Promise<!Blob>} Promise of the thumbnail as a jpeg blob.
 */
export async function scaleVideo(blob, width, height = undefined) {
  const el = await loadVideoBlob(blob);
  if (height === undefined) {
    height = Math.round(width * el.videoHeight / el.videoWidth);
  }
  return elementToJpegBlob(el, width, height);
}

/**
 * Creates a thumbnail of image by scaling it to the target size.
 * @param {!Blob} blob Blob of image to be scaled.
 * @param {number} width Target width.
 * @param {number=} height Target height. Preserve the aspect ratio if not set.
 * @return {!Promise<!Blob>} Promise of the thumbnail as a jpeg blob.
 */
export async function scaleImage(blob, width, height = undefined) {
  const el = await loadImageBlob(blob);
  if (height === undefined) {
    height = Math.round(width * el.naturalHeight / el.naturalWidth);
  }
  return elementToJpegBlob(el, width, height);
}
