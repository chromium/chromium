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
     * Current window states.
     * @type {!Array<!chromeosCamera.mojom.WindowStateType>}
     */
    this.windowStates_ = [];
  }

  /** @override */
  async bind(remoteController) {
    this.windowStateController_ = remoteController;

    const windowMonitorCallbackRouter =
        new chromeosCamera.mojom.WindowStateMonitorCallbackRouter();
    windowMonitorCallbackRouter.onWindowStateChanged.addListener((states) => {
      this.windowStates_ = states;
    });
    const {states} = await this.windowStateController_.addMonitor(
        windowMonitorCallbackRouter.$.bindNewPipeAndPassRemote());
    this.windowStates_ = states;
  }

  /** @override */
  async minimize() {
    return assertInstanceof(
               this.windowStateController_,
               chromeosCamera.mojom.WindowStateControllerRemote)
        .minimize();
  }

  /** @override */
  async maximize() {
    return assertInstanceof(
               this.windowStateController_,
               chromeosCamera.mojom.WindowStateControllerRemote)
        .maximize();
  }

  /** @override */
  async restore() {
    return assertInstanceof(
               this.windowStateController_,
               chromeosCamera.mojom.WindowStateControllerRemote)
        .restore();
  }

  /** @override */
  async fullscreen() {
    return assertInstanceof(
               this.windowStateController_,
               chromeosCamera.mojom.WindowStateControllerRemote)
        .fullscreen();
  }

  /** @override */
  async focus() {
    return assertInstanceof(
               this.windowStateController_,
               chromeosCamera.mojom.WindowStateControllerRemote)
        .focus();
  }

  /** @override */
  isMinimized() {
    return this.windowStates_.includes(
        chromeosCamera.mojom.WindowStateType.MINIMIZED);
  }

  /** @override */
  isFullscreenOrMaximized() {
    return this.windowStates_.includes(
               chromeosCamera.mojom.WindowStateType.FULLSCREEN) ||
        this.windowStates_.includes(
            chromeosCamera.mojom.WindowStateType.MAXIMIZED);
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
