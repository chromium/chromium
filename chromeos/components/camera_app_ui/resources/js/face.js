// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof, assertNotReached} from './chrome_util.js';
import * as dom from './dom.js';

// eslint-disable-next-line no-unused-vars
import {Resolution} from './type.js';

// G-yellow-600 with alpha = 0.8
const RECT_COLOR = 'rgba(249, 171, 0, 0.8)';

/**
 * Rotates the given coordinates in [0, 1] square space by the given
 * orientation.
 * @param {number} x
 * @param {number} y
 * @param {number} orientation
 * @return {!Array<number>} The rotated [x, y].
 */
function rotate(x, y, orientation) {
  switch (orientation) {
    case 0:
      return [x, y];
    case 90:
      return [y, 1.0 - x];
    case 180:
      return [1.0 - x, 1.0 - y];
    case 270:
      return [1.0 - y, x];
  }
  assertNotReached('Unexpected orientation');
}

/**
 * An overlay to show face rectangles over preview.
 */
export class FaceOverlay {
  /**
   * @param {!Resolution} activeArraySize
   * @param {number} sensorOrientation
   */
  constructor(activeArraySize, sensorOrientation) {
    /**
     * @const {!Resolution}
     * @private
     */
    this.activeArraySize_ = activeArraySize;

    /**
     * @const {number}
     * @private
     */
    this.sensorOrientation_ = sensorOrientation;

    /**
     * @const {!HTMLCanvasElement}
     * @private
     */
    this.canvas_ = dom.get('#preview-face-overlay', HTMLCanvasElement);

    /**
     * @const {!CanvasRenderingContext2D}
     * @private
     */
    this.ctx_ = assertInstanceof(
        this.canvas_.getContext('2d'), CanvasRenderingContext2D);
  }

  /**
   * Shows the given rectangles on overlay. The old rectangles would be
   * cleared, if any.
   * @param {!Array<number>} rects An array of [x1, y1, x2, y2] to represent
   *     rectangles in the coordinate system of active array in sensor.
   */
  show(rects) {
    this.ctx_.clearRect(0, 0, this.canvas_.width, this.canvas_.height);

    // TODO(b/178344897): Handle zoomed preview.
    // TODO(b/178344897): Handle cropped preview.
    // TODO(b/178344897): Handle screen orientation.

    this.ctx_.strokeStyle = RECT_COLOR;
    for (let i = 0; i < rects.length; i += 4) {
      let [x1, y1, x2, y2] = rects.slice(i, i + 4);
      x1 /= this.activeArraySize_.width;
      y1 /= this.activeArraySize_.height;
      x2 /= this.activeArraySize_.width;
      y2 /= this.activeArraySize_.height;
      [x1, y1] = rotate(x1, y1, this.sensorOrientation_);
      [x2, y2] = rotate(x2, y2, this.sensorOrientation_);
      x1 *= this.canvas_.width;
      y1 *= this.canvas_.height;
      x2 *= this.canvas_.width;
      y2 *= this.canvas_.height;
      this.ctx_.strokeRect(x1, y1, x2 - x1, y2 - y1);
    }
  }

  /**
   * Clears all rectangles.
   */
  clear() {
    this.ctx_.clearRect(0, 0, this.canvas_.width, this.canvas_.height);
  }
}
