// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../chrome_util.js';

// eslint-disable-next-line no-unused-vars
import {WindowController} from './window_controller_interface.js';

/**
 * WindowController which relies on our specific Mojo interface.
 * @implements {WindowController}
 */
export class MojoWindowController {
  /**
   * @public
   */
  constructor() {
    /**
     * The remote controller from Mojo interface.
     * @type {?chromeosCamera.mojom.WindowStateControllerRemote}
     */
    this.windowStateController_ = null;

    /**
     * Current window state.
     * @type {?chromeosCamera.mojom.WindowStateType}
     */
    this.windowState_ = null;
  }

  /** @override */
  async bind(remoteController) {
    this.windowStateController_ = remoteController;

    const windowMonitorCallbackRouter =
        new chromeosCamera.mojom.WindowStateMonitorCallbackRouter();
    windowMonitorCallbackRouter.onWindowStateChanged.addListener((state) => {
      this.windowState_ = state;
    });
    const {state} = await this.windowStateController_.addMonitor(
        windowMonitorCallbackRouter.$.bindNewPipeAndPassRemote());
    this.windowState_ = state;
  }

  /** @override */
  async minimize() {
    assertInstanceof(
        this.windowStateController_,
        chromeosCamera.mojom.WindowStateControllerRemote)
        .minimize();
  }

  /** @override */
  async maximize() {
    assertInstanceof(
        this.windowStateController_,
        chromeosCamera.mojom.WindowStateControllerRemote)
        .maximize();
  }

  /** @override */
  async restore() {
    assertInstanceof(
        this.windowStateController_,
        chromeosCamera.mojom.WindowStateControllerRemote)
        .restore();
  }

  /** @override */
  async fullscreen() {
    assertInstanceof(
        this.windowStateController_,
        chromeosCamera.mojom.WindowStateControllerRemote)
        .fullscreen();
  }

  /** @override */
  async focus() {
    assertInstanceof(
        this.windowStateController_,
        chromeosCamera.mojom.WindowStateControllerRemote)
        .focus();
  }

  /** @override */
  isMinimized() {
    return this.windowState_ === chromeosCamera.mojom.WindowStateType.MINIMIZED;
  }

  /** @override */
  isFullscreenOrMaximized() {
    return this.windowState_ ===
        chromeosCamera.mojom.WindowStateType.FULLSCREEN ||
        this.windowState_ === chromeosCamera.mojom.WindowStateType.MAXIMIZED;
  }

  /** @override */
  enable() {
    // TODO(980846): Implement by hiding "Camera is in-use" message to user.
  }

  /** @override */
  disable() {
    // TODO(980846): Implement by showing "Camera is in-use" message to user.
  }

  /** @override */
  addOnMinimizedListener(listener) {
    // TODO(980846): Remove the method once we migrate to SWA.
  }
}

export const windowController = new MojoWindowController();
