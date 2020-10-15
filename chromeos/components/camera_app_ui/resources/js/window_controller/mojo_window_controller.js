// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {NotImplementedError} from '../error.js';

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
  constructor() {}

  /** @override */
  async minimize() {
    throw new NotImplementedError();
  }

  /** @override */
  async maximize() {
    throw new NotImplementedError();
  }

  /** @override */
  async restore() {
    throw new NotImplementedError();
  }

  /** @override */
  async fullscreen() {
    throw new NotImplementedError();
  }

  /** @override */
  async focus() {
    throw new NotImplementedError();
  }

  /** @override */
  isMinimized() {
    // TODO(980846): Implement the minimization monitor.
    return false;
  }

  /** @override */
  isFullscreenOrMaximized() {
    // TODO(980846): Implement the fullscreen monitor.
    return false;
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
