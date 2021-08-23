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
 *   gaia: string,
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
 *   userGaia: string,
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
   * @return {Promise<bool>}
   */
  canStartProjectorSession() {}

  /**
   * Launches the Projector recording session for gaiaId. Returns true if
   * a projector recording session was successfully launched.
   * @param {string} gaiaId, user's gaia id.
   * @param {Promise<bool>}
   */
  startProjectorSession(gaiaId) {}

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} gaiaId, user's gaia id.
   * @return {!Promise<OAuthToken>}
   */
  getOAuthTokenForAccount(gaiaId) {}

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {Array<string>} msg Error messages.
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
  startProjectorSession(gaiaId) {
    return sendWithPromise('startProjectorSession', [gaiaId]);
  }

  /** @override */
  getOAuthTokenForAccount(gaiaId) {
    return sendWithPromise('getOAuthTokenForAccount', [gaiaId]);
  }

  /** @override */
  onError(msg) {
    return chrome.send(err, [msg])
  }
}

addSingletonGetter(ProjectorBrowserProxyImpl);
