// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {CanvasRenderingContext2D}
 */
export class FakeCanvasContext {
  constructor() {
    this.clearRectCalls_ = [];
    this.fillRectCalls_ = [];
  }

  /** @override */
  clearRect(x, y, width, height) {
    this.clearRectCalls_.push([x, y, width, height]);
  }

  /** @override */
  fillRect(x, y, width, height) {
    this.fillRectCalls_.push([x, y, width, height]);
  }

  /**
   * @return {Array<Array<int>}
   */
  getClearRectCalls() {
    return this.clearRectCalls_;
  }

  /**
   * @return {Array<Array<int>}
   */
  getFillRectCalls() {
    return this.fillRectCalls_;
  }
}
