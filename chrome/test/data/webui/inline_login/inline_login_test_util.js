// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InlineLoginBrowserProxy} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @return {!Array<string>} */
export function getFakeAccountsList() {
  return ['test@gmail.com', 'test2@gmail.com', 'test3@gmail.com'];
}

export class TestAuthenticator extends EventTarget {
  constructor() {
    super();
    // Note: We cannot import types from authenticator.m.js because we replace
    // "chrome://chrome-signin/" with "chrome/browser/resources/inline_login/"
    // and authenticator is in "chrome/browser/resources/gaia_auth_host/"
    // folder.

    /**
     * Type AuthMode (see Authenticator).
     * @type {?Object}
     */
    this.authMode = null;
    /**
     * Type AuthParams (see Authenticator).
     * @type {?Object}
     */
    this.data = null;
    /** @type {number} */
    this.loadCalls = 0;
    /** @type {number} */
    this.getAccountsResponseCalls = 0;
    /** @type {Array<string>} */
    this.getAccountsResponseResult = null;
  }

  /**
   * @param {Object} authMode Authorization mode (type AuthMode).
   * @param {Object} data Parameters for the authorization flow (type
   *     AuthParams).
   */
  load(authMode, data) {
    this.loadCalls++;
    this.authMode = authMode;
    this.data = data;
  }

  /**
   * @param {Array<string>} accounts list of emails.
   */
  getAccountsResponse(accounts) {
    this.getAccountsResponseCalls++;
    this.getAccountsResponseResult = accounts;
  }
}

/** @implements {InlineLoginBrowserProxy} */
export class TestInlineLoginBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initialize',
      'authExtensionReady',
      'switchToFullTab',
      'completeLogin',
      'lstFetchResults',
      'metricsHandler:recordAction',
      'showIncognito',
      'getAccounts',
      'dialogClose',
    ]);
  }

  /** @override */
  initialize() {
    this.methodCalled('initialize');
  }

  /** @override */
  authExtensionReady() {
    this.methodCalled('authExtensionReady');
  }

  /** @override */
  switchToFullTab(url) {
    this.methodCalled('switchToFullTab', url);
  }

  /** @override */
  completeLogin(credentials) {
    this.methodCalled('completeLogin', credentials);
  }

  /** @override */
  lstFetchResults(arg) {
    this.methodCalled('lstFetchResults', arg);
  }

  /** @override */
  recordAction(metricsAction) {
    this.methodCalled('metricsHandler:recordAction', metricsAction);
  }

  /** @override */
  showIncognito() {
    this.methodCalled('showIncognito');
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve(getFakeAccountsList());
  }

  /** @override */
  dialogClose() {
    this.methodCalled('dialogClose');
  }
}
