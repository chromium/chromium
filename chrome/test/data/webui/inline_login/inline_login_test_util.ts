// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="chromeos_ash">
import {AccountAdditionOptions} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
// </if>

import {InlineLoginBrowserProxy} from 'chrome://chrome-signin/inline_login_browser_proxy.js';
import {AuthCompletedCredentials, AuthMode, AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';

import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export function getFakeAccountsList(): string[] {
  return ['test@gmail.com', 'test2@gmail.com', 'test3@gmail.com'];
}

export function getFakeDeviceId(): string {
  return '4b1918ab-e8a4-456d-b499-0000deadbeef';
}

export const fakeAuthenticationData = {
  hl: 'hl',
  gaiaUrl: 'https://accounts.google.com/',
  gaiaPath: 'gaiaPath',
  authMode: 1,
};

export const fakeAuthenticationDataWithEmail = {
  hl: 'hl',
  gaiaUrl: 'https://accounts.google.com/',
  gaiaPath: 'gaiaPath',
  authMode: 1,
  email: 'example@gmail.com',
};

/*
 * Fake data used for `show-signin-error-page` web listener in
 * chrome/browser/resources/inline_login/inline_login_app.js.
 */
export const fakeSigninBlockedByPolicyData = {
  email: 'john.doe@example.com',
  hostedDomain: 'example.com',
  deviceType: 'Chromebook',
  signinBlockedByPolicy: true,
};

export class TestAuthenticator extends EventTarget {
  authMode: AuthMode|null = null;
  data: AuthParams|null = null;
  loadCalls: number = 0;
  getAccountsResponseCalls: number = 0;
  getAccountsResponseResult: string[]|null = null;
  getDeviceIdResponseCalls: number = 0;
  getDeviceIdResponseResult: string = '';

  /**
   * @param authMode Authorization mode.
   * @param data Parameters for the authorization flow.
   */
  load(authMode: AuthMode, data: AuthParams) {
    this.loadCalls++;
    this.authMode = authMode;
    this.data = data;
  }

  /**
   * @param accounts list of emails.
   */
  getAccountsResponse(accounts: string[]) {
    this.getAccountsResponseCalls++;
    this.getAccountsResponseResult = accounts;
  }

  /**
   * @param deviceId Device ID.
   */
  getDeviceIdResponse(deviceId: string) {
    this.getDeviceIdResponseCalls++;
    this.getDeviceIdResponseResult = deviceId;
  }
}

export class TestInlineLoginBrowserProxy extends TestBrowserProxy implements
    InlineLoginBrowserProxy {
  // <if expr="chromeos_ash">
  private dialogArguments_: AccountAdditionOptions|null = null;
  // </if>

  constructor() {
    super([
      'initialize', 'authenticatorReady', 'switchToFullTab', 'completeLogin',
      'lstFetchResults', 'metricsHandler:recordAction', 'showIncognito',
      'getAccounts', 'getDeviceId', 'dialogClose',
      // <if expr="chromeos_ash">
      'skipWelcomePage', 'openGuestWindow', 'getDialogArguments',
      // </if>
    ]);
  }

  // <if expr="chromeos_ash">
  setDialogArguments(dialogArguments: AccountAdditionOptions|null) {
    this.dialogArguments_ = dialogArguments;
  }
  // </if>

  initialize() {
    this.methodCalled('initialize');
  }

  authenticatorReady() {
    this.methodCalled('authenticatorReady');
  }

  switchToFullTab(url: string) {
    this.methodCalled('switchToFullTab', url);
  }

  completeLogin(credentials: AuthCompletedCredentials) {
    this.methodCalled('completeLogin', credentials);
  }

  lstFetchResults(arg: string) {
    this.methodCalled('lstFetchResults', arg);
  }

  recordAction(metricsAction: string) {
    this.methodCalled('metricsHandler:recordAction', metricsAction);
  }

  showIncognito() {
    this.methodCalled('showIncognito');
  }

  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve(getFakeAccountsList());
  }

  getDeviceId() {
    this.methodCalled('getDeviceId');
    return Promise.resolve(getFakeDeviceId());
  }

  dialogClose() {
    this.methodCalled('dialogClose');
  }

  // <if expr="chromeos_ash">
  skipWelcomePage(skip: boolean) {
    this.methodCalled('skipWelcomePage', skip);
  }

  openGuestWindow() {
    this.methodCalled('openGuestWindow');
  }

  getDialogArguments() {
    return JSON.stringify(this.dialogArguments_);
  }
  // </if>
}
