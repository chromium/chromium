// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {EduCoexistenceBrowserProxy} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_browser_proxy.js';
import type {EduCoexistenceParams} from 'chrome://chrome-signin/edu_coexistence/edu_coexistence_controller.js';
import type {AuthCompletedCredentials} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

const DEFAULT_COEXISTENCE_PARAMS = {
  url: 'https://foo.example.com/supervision/coexistence/intro',
  hl: 'en-US',
  sourceUi: 'oobe',
  clientId: 'test-client-id',
  clientVersion: ' test-client-version',
  eduCoexistenceId: ' test-edu-coexistence-id',
  platformVersion: ' test-platform-version',
  releaseChannel: 'test-release-channel',
  deviceId: 'test-device-id',
  eduCoexistenceAccessToken: 'token',
  signinTime: 10000,
};

export class TestEduCoexistenceBrowserProxy extends TestBrowserProxy implements
    EduCoexistenceBrowserProxy {
  private coexistenceParams: EduCoexistenceParams;

  constructor() {
    super([
      'initializeLogin',
      'initializeEduArgs',
      'authenticatorReady',
      'completeLogin',
      'getAccounts',
      'getDeviceId',
      'consentValid',
      'consentLogged',
      'dialogClose',
      'onError',
    ]);

    this.coexistenceParams = DEFAULT_COEXISTENCE_PARAMS;
  }

  initializeLogin() {
    this.methodCalled('initializeLogin');
  }

  initializeEduArgs(): Promise<EduCoexistenceParams> {
    this.methodCalled('initializeEduArgs');
    return Promise.resolve(this.coexistenceParams);
  }

  authenticatorReady() {
    this.methodCalled('authenticatorReady');
  }

  completeLogin(credentials: AuthCompletedCredentials) {
    this.methodCalled('completeLogin', credentials);
  }

  getAccounts(): Promise<string[]> {
    this.methodCalled('getAccounts');
    return Promise.resolve(
        ['test@gmail.com', 'test2@gmail.com', 'test3@gmail.com']);
  }

  getDeviceId(): Promise<string> {
    this.methodCalled('getDeviceId');
    return Promise.resolve('4b1918ab-e8a4-456d-b499-0000deadbeef');
  }

  consentValid() {
    this.methodCalled('consentValid');
  }

  consentLogged(account: string, eduCoexistenceTosVersion: string):
      Promise<boolean> {
    this.methodCalled('consentLogged', account, eduCoexistenceTosVersion);
    return Promise.resolve(true);
  }

  dialogClose() {
    this.methodCalled('dialogClose');
  }

  onError() {
    this.methodCalled('error');
  }

  /**
   * The passed in params override DEFAULT_COEXISTENCE_PARAMS for this instance
   * of TestEduCoexistenceBrowserProxy.
   */
  setCoexistenceParams(coexistenceParams: EduCoexistenceParams) {
    this.coexistenceParams = coexistenceParams;
  }
}
