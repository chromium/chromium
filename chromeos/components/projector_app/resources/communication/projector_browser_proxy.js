// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * To use the browser proxy, please import this module and call
 * ProjectorBrowserProxyImpl.getInstance().*
 *
 * @interface
 */
export class ProjectorBrowserProxy {
  /**
   * Notifies the embedder content that tool has been set for annotator.
   * @param {!projectorApp.AnnotatorToolParams} tool
   */
  onToolSet(tool) {}

  /**
   * Notifies the embedder content that undo/redo availability changed for
   * annotator.
   * @param {boolean} undoAvailable
   * @param {boolean} redoAvailable
   */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {}

  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<!projectorApp.Account>>}
   */
  getAccounts() {}

  /**
   * Checks whether the SWA can trigger a new Projector session.
   * @return {Promise<boolean>}
   */
  canStartProjectorSession() {}

  /**
   * Launches the Projector recording session. Returns true if a projector
   * recording session was successfully launched.
   * @param {string} storageDir, the directory name in which the screen cast
   *     will be saved in.
   * @return {Promise<boolean>}
   */
  startProjectorSession(storageDir) {}

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} email, user's email.
   * @return {!Promise<!projectorApp.OAuthToken>}
   */
  getOAuthTokenForAccount(email) {}

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<string>} msg Error messages.
   */
  onError(msg) {}
}

/**
 * @implements {ProjectorBrowserProxy}
 */
export class ProjectorBrowserProxyImpl {
  /** @override */
  onToolSet(tool) {
    return chrome.send('onToolSet', [tool]);
  }

  /** @override */
  onUndoRedoAvailabilityChanged(undoAvailable, redoAvailable) {
    return chrome.send(
        'onUndoRedoAvailabilityChanged', [undoAvailable, redoAvailable]);
  }

  /** @override */
  getAccounts() {
    return sendWithPromise('getAccounts');
  }

  /** @override */
  canStartProjectorSession() {
    return sendWithPromise('canStartProjectorSession');
  }

  /** @override */
  startProjectorSession(storageDir ) {
    return sendWithPromise('startProjectorSession', [storageDir]);
  }

  /** @override */
  getOAuthTokenForAccount(email) {
    return sendWithPromise('getOAuthTokenForAccount', [email]);
  }

  /** @override */
  onError(msg) {
    return chrome.send("onError", msg)
  }
}

addSingletonGetter(ProjectorBrowserProxyImpl);
