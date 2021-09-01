// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * Types of ProjectorError.
 * @enum {string}
 */
export const ProjectorError = {
  NONE: 'NONE',
  TOKEN_FETCH_FAILURE: 'TOKEN_FETCH_FAILURE',
  TOKEN_FETCH_ALREADY_IN_PROGRESS: 'TOKEN_FETCH_ALREADY_IN_PROGRESS',
  OTHER: 'OTHER',
};

/**
 * Account passed when getAccounts is called.
 * @typedef {{
 *   name: string,
 *   email: string,
 *   pictureURL: string,
 *   isPrimaryUser: boolean
 * }}
 */
export let Account;

/**
 * Account passed when getAccounts is called.
 * @typedef {{
 *   token: string,
 *   expirationTime: string,
 * }}
 */
export let OAuthTokenInfo;

/**
 * Oauth token returned when getOAuthTokenForAccount is called.
 * @typedef {{
 *   email: string,
 *   oauthTokenInfo: OAuthTokenInfo,
 *   error: ProjectorError
 * }}
 */
export let OAuthToken;


/**
 * To use the browser proxy, please import this module and call
 * ProjectorBrowserProxyImpl.getInstance().*
 *
 * @interface
 */
export class ProjectorBrowserProxy {
  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<Account>>}
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
   * @return {!Promise<OAuthToken>}
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
