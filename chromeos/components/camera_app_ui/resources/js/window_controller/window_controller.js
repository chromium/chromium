// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {WindowController} from './window_controller_interface.js';

/**
 * Changes the state of the current App window.
 * @param {function(!chrome.app.window.AppWindow): boolean} predicate The
 *     function to determine whether the window is in the target state.
 * @param {function(!chrome.app.window.AppWindow): !chrome.events.Event}
 *     getEventTarget The function to get the target for adding the event
 *     listener.
 * @param {function(!chrome.app.window.AppWindow): undefined} changeState The
 *     function to trigger the state change of the window.
 * @return {!Promise<undefined>} A completion Promise that will be resolved
 *     when the window is in the target state.
 */
function changeWindowState(predicate, getEventTarget, changeState) {
  const win = chrome.app.window.current();
  const eventTarget = getEventTarget(win);
  return new Promise((resolve) => {
    if (predicate(win)) {
      resolve();
      return;
    }
    const onStateChanged = () => {
      eventTarget.removeListener(onStateChanged);
      resolve();
    };
    eventTarget.addListener(onStateChanged);
    changeState(win);
  });
}

/**
 * WindowController which mainly relies on Chrome AppWindow API.
 * @implements {WindowController}
 */
export class ChromeWindowController {
  /** @override */
  async bind(remoteController) {
    // We control the window uses Chrome AppWindow API directly for platform
    // app. There is no need to bind to the implementation through Mojo.
  }

  /** @override */
  async minimize() {
    changeWindowState(
        (w) => w.isMinimized(), (w) => w.onMinimized, (w) => w.minimize());
  }

  /** @override */
  async maximize() {
    changeWindowState(
        (w) => w.isMaximized(), (w) => w.onMaximized, (w) => w.maximize());
  }

  /** @override */
  async restore() {
    changeWindowState(
        (w) => !w.isMaximized() && !w.isMinimized() && !w.isFullscreen(),
        (w) => w.onRestored, (w) => w.restore());
    chrome.app.window.current().show();
  }

  /** @override */
  async fullscreen() {
    changeWindowState(
        (w) => w.isFullscreen(), (w) => w.onFullscreened,
        (w) => w.fullscreen());
  }

  /** @override */
  async focus() {
    changeWindowState(
        (w) => w.contentWindow.document.hasFocus(),
        ({contentWindow: cw}) => ({
          addListener: cw.addEventListener.bind(cw, 'focus'),
          removeListener: cw.removeEventListener.bind(cw, 'focus'),
        }),
        (w) => w.focus());
  }

  /** @override */
  isMinimized() {
    return chrome.app.window.current().isMinimized();
  }

  /** @override */
  isFullscreenOrMaximized() {
    return chrome.app.window.current().outerBounds.width >= screen.width ||
        chrome.app.window.current().outerBounds.height >= screen.height;
  }

  /** @override */
  enable() {
    chrome.app.window.current().show();
  }

  /** @override */
  disable() {
    chrome.app.window.current().hide();
  }

  /** @override */
  addOnMinimizedListener(listener) {
    chrome.app.window.current().onMinimized.addListener(listener);
  }
}

export const windowController = new ChromeWindowController();
