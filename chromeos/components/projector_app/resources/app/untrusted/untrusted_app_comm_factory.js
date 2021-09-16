// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome-untrusted://projector/js/post_message_api_client.m.js';
import {RequestHandler} from 'chrome-untrusted://projector/js/post_message_api_request_handler.m.js';

const TARGET_URL = 'chrome://projector/'

// A client that sends notification to the chrome://projector embedder.
export class TrustedAppClient extends PostMessageAPIClient {
  /**
   * @param {!Window} parentWindow The embedder window from which requests
   *     come.
   */
  constructor(parentWindow) {
    super(TARGET_URL, parentWindow);
  }

  /**
   * Gets the list of primary and secondary accounts currently available on the
   * device.
   * @return {Promise<Array<!projectorApp.Account>>}
   */
  getAccounts() {
    return this.callApiFn('getAccounts', []);
  }

  /**
   * Checks whether the SWA can trigger a new Projector session.
   * @return {Promise<boolean>}
   */
  canStartProjectorSession() {
    return this.callApiFn('canStartProjectorSession', []);
  }

  /**
   * Starts the Projector session if it is possible. Provides the storage dir
   * where  projector output artifacts will be saved in.
   * @param {string} storageDir
   * @return {Promise<boolean>}
   */
  startProjectorSession(storageDir) {
    return this.callApiFn('startProjectorSession', [storageDir]);
  }

  /**
   * Gets the oauth token with the required scopes for the specified account.
   * @param {string} account user's email
   * @return {!Promise<!projectorApp.OAuthToken>}
   */
  getOAuthTokenForAccount(account) {
    return this.callApiFn('getOAuthTokenForAccount', [account]);
  }

  /**
   * Sends 'error' message to handler.
   * The Handler will log the message. If the error is not a recoverable error,
   * the handler closes the corresponding WebUI.
   * @param {!Array<ProjectorError>} msg Error messages.
   */
  onError(msg) {
    this.callApiFn('onError', [msg]);
  }
};


/**
 * Class that implements the RequestHandler inside the Projector untrusted
 * scheme for projector app.
 */
export class UntrustedAppRequestHandler extends RequestHandler {
  /**
   * @param {!Window} parentWindow  The embedder window from which requests
   *     come.
   */
  constructor(parentWindow) {
    super(null, TARGET_URL, TARGET_URL);
    this.targetWindow_ = parentWindow;

    this.registerMethod('onCanStartNewSession', (canStart) => {
      // TODO(b/196245932) Call into the projector app externs to notify it on
      // whether it can start a new session.
      return true;
    });
  }

  /** @override */
  targetWindow() {
    return this.targetWindow_;
  }
};


/**
 * This is a class that is used to setup the duplex communication channels
 * between this origin, chrome-untrusted://projector/* and the embedder content.
 */
export class AppUntrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and Requesthandler.
   */
  static maybeCreateInstances() {
    if (AppUntrustedCommFactory.client_ ||
        AppUntrustedCommFactory.requestHandler_) {
      return;
    }

    AppUntrustedCommFactory.client_ = new TrustedAppClient(window.parent);

    AppUntrustedCommFactory.requestHandler_ =
        new UntrustedAppRequestHandler(window.parent);
  }

  /**
   * In order to use this class, please do the following (e.g. to check if it is
   * possible to start a new ProjectorSession):
   * const canStart = await AppUntrustedCommFactory. getPostMessageAPIClient().
   *     canStartProjectorSession();
   * @return {!TrustedAppClient}
   */
  static getPostMessageAPIClient() {
    // .AppUntrustedCommFactory.client_ should be available. However to be on
    // the cautious side create an instance here if getPostMessageAPIClient is
    // triggered before the page finishes loading.
    AppUntrustedCommFactory.maybeCreateInstances();
    return AppUntrustedCommFactory.client_;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Create instances of the singletons (PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AppUntrustedCommFactory.maybeCreateInstances();
});
