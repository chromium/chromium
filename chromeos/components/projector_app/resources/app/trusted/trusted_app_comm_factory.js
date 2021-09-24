// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PostMessageAPIClient} from 'chrome://resources/js/post_message_api_client.m.js';
import {RequestHandler} from 'chrome://resources/js/post_message_api_request_handler.m.js';

import {ProjectorBrowserProxy, ProjectorBrowserProxyImpl} from '../../communication/projector_browser_proxy.js';

const TARGET_URL = 'chrome-untrusted://projector/'


// A PostMessageAPIClient that sends messages to chrome-untrusted://projector.
export class UntrustedAppClient extends PostMessageAPIClient {
  /**
   * @param {!Window} targetWindow
   */
  constructor(targetWindow) {
    super(TARGET_URL, targetWindow);
  }

  /**
   * Notfies the app whether it can start a new session or not.
   * @param {!boolean} canStart
   * @return {Promise<boolean>}
   */
  onNewScreencastPreconditionChanged(canStart) {
    return this.callApiFn('onNewScreencastPreconditionChanged', [canStart]);
  }
}

/**
 * Class that implements the RequestHandler inside the Projector trusted scheme
 * for the Projector App.
 */
export class TrustedAppRequestHandler extends RequestHandler {
  /*
   * @param {!Element} iframeElement The <iframe> element to listen to as a
   *     client.
   * @param {ProjectorBrowserProxy} browserProxy The browser proxy that will be
   *     used to handle the messages.
   */
  constructor(iframeElement, browserProxy) {
    super(iframeElement, TARGET_URL, TARGET_URL);
    this.browserProxy_ = browserProxy;

    this.registerMethod('getAccounts', (args) => {
      return this.browserProxy_.getAccounts();
    });
    this.registerMethod('canStartProjectorSession', (args) => {
      return this.browserProxy_.canStartProjectorSession();
    });
    this.registerMethod('startProjectorSession', (storageDir) => {
      if (!storageDir || storageDir.length != 1) {
        return false;
      }
      return this.browserProxy_.startProjectorSession(storageDir[0]);
    });
    this.registerMethod('getOAuthTokenForAccount', (account) => {
      if (!account || account.length != 1) {
        return {};
      }
      return this.browserProxy_.getOAuthTokenForAccount(account[0]);
    });
    this.registerMethod('onError', (msg) => {
      this.browserProxy_.onError(msg);
    });
  }
};

/**
 * This is a class that is used to setup the duplex communication
 * channels between this origin, chrome://projector/* and the iframe embedded
 * inside the document.
 */
export class AppTrustedCommFactory {
  /**
   * Creates the instances of PostMessageAPIClient and RequestHandler.
   */
  static maybeCreateInstances() {
    if (AppTrustedCommFactory.client_ ||
        AppTrustedCommFactory.requestHandler_) {
      return;
    }

    let iframeElement = document.getElementsByTagName('iframe')[0];

    AppTrustedCommFactory.client_ =
        new UntrustedAppClient(iframeElement.contentWindow);

    AppTrustedCommFactory.requestHandler_ = new TrustedAppRequestHandler(
        iframeElement, ProjectorBrowserProxyImpl.getInstance());
  }

  /**
   * In order to use this class, please do the following (e.g. to notify the app
   * that it can start a new session):
   * const success = await AppTrustedCommFactory.
   *                 getPostMessageAPIClient().onCanStartNewSession(true);
   * @return {!UntrustedAppClient}
   */
  static getPostMessageAPIClient() {
    // AnnotatorUntrustedCommFactory.client_ should be available. However to be
    // on the cautious side create an instance here if getPostMessageAPIClient
    // is triggered before the page finishes loading.
    AppTrustedCommFactory.maybeCreateInstances();
    return AppTrustedCommFactory.client_;
  }
}

document.addEventListener('DOMContentLoaded', () => {
  // Create instances of the singletons (PostMessageAPIClient and
  // RequestHandler) when the document has finished loading.
  AppTrustedCommFactory.maybeCreateInstances();
});
