// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from './dom.js';
import {
  EmptyThumbnailError,
  LoadError,
  PlayError,
  PlayMalformedError,
} from './type.js';
import {newDrawingCanvas} from './util.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * Converts the element to a jpeg blob by drawing it on a canvas.
 * @param {!CanvasImageSource} element Source element.
 * @param {number} width Canvas width.
 * @param {number} height Canvas height.
 * @return {!Promise<!Blob>} Converted jpeg blob.
 * @throws {!EmptyThumbnailError} Thrown when the data to generate thumbnail is
 *     empty.
 */
async function elementToJpegBlob(element, width, height) {
  const {canvas, ctx} = newDrawingCanvas({width, height});
  ctx.drawImage(element, 0, 0, width, height);

  /**
   * @type {!Uint8ClampedArray} A one-dimensional pixels array in RGBA order.
   */
  const data = ctx.getImageData(0, 0, width, height).data;
  if (data.every((byte) => byte === 0)) {
    throw new EmptyThumbnailError();
  }

  return new Promise((resolve) => {
    canvas.toBlob(resolve, 'image/jpeg');
  });
}

/**
 * Loads the blob into a <video> element.
 * @param {!Blob} blob
 * @return {!Promise<!HTMLVideoElement>}
 * @throws {!Error} Thrown when it fails to load video.
 */
async function loadVideoBlob(blob) {
  const el = dom.create('video', HTMLVideoElement);
  try {
    const hasLoaded = new WaitableEvent();
    el.addEventListener('error', () => {
      hasLoaded.signal(false);
    });
    el.addEventListener('loadeddata', () => {
      hasLoaded.signal(true);
    });
    const gotFrame = new WaitableEvent();
    el.requestVideoFrameCallback(() => gotFrame.signal());
    el.preload = 'auto';
    el.src = URL.createObjectURL(blob);
    if (!(await hasLoaded.wait())) {
      throw new LoadError(el.error.message);
    }

    try {
      await el.play();
    } catch (e) {
      throw new PlayError(e.message);
    }

    try {
      // The |requestVideoFrameCallback| may not be triggered when playing
      // malformed video. Set 300ms timeout here to prevent UI be blocked
      // forever.
      await gotFrame.timedWait(300);
    } catch (e) {
      throw new PlayMalformedError(e.message);
    } finally {
      el.pause();
    }
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
