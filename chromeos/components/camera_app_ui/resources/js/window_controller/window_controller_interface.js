// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller to get/set/listener for window state.
 * @interface
 */
export class WindowController {
  /**
   * Binds the controller remote from Mojo interface.
   * @param {!chromeosCamera.mojom.WindowStateControllerRemote} remoteController
   * @return {!Promise}
   * @abstract
   */
  async bind(remoteController) {}

  /**
   * Minimizes the window.
   * @return {!Promise}
   * @abstract
   */
  async minimize() {}

  /**
   * Maximizes the window.
   * @return {!Promise}
   * @abstract
   */
  async maximize() {}

  /**
   * Restores the window and leaves maximized/minimized/fullscreen state.
   * @return {!Promise}
   * @abstract
   */
  async restore() {}

  /**
   * Makes the window fullscreen.
   * @return {!Promise}
   * @abstract
   */
  async fullscreen() {}

  /**
   * Focuses the window.
   * @return {!Promise}
   * @abstract
   */
  async focus() {}

  /**
   * Returns true if the window is currently minimized.
   * @return {boolean}
   * @abstract
   */
  isMinimized() {}

  /**
   * Returns true if the window is currently fullscreen or maximized.
   * @return {boolean}
   * @abstract
   */
  isFullscreenOrMaximized() {}

  /**
   * Enables the window.
   * @abstract
   */
  enable() {}

  /**
   * Disables the window.
   * @abstract
   */
  disable() {}

  /**
   * Adds listener for window minimization event.
   * @param {function(): void} listener
   * @abstract
   */
  addOnMinimizedListener(listener) {}
}
