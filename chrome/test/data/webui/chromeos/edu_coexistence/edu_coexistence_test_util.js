// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../../test_browser_proxy.m.js';

/** @return {!Array<string>} */
export function getFakeAccountsList() {
  return ['test@gmail.com', 'test2@gmail.com', 'test3@gmail.com'];
}

/** @implements {EduCoexistenceBrowserProxy} */
export class TestEduCoexistenceBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'initializeLogin',
      'initializeEduArgs',
      'completeLogin',
      'getAccounts',
      'consentValid',
      'consentLogged',
      'dialogClose',
      'error',
    ]);
  }

  /** @override */
  initializeLogin() {
    this.methodCalled('initializeLogin');
  }

  /** @override */
  initializeEduArgs() {
    this.methodCalled('initializeEduArgs');
    return this.initializeEduArgsResponse_ ? this.initializeEduArgsResponse_() :
                                             Promise.resolve({});
  }

  /** @param {function} initializeEduArgsResponse */
  setInitializeEduArgsResponse(initializeEduArgsResponse) {
    this.initializeEduArgsResponse_ = initializeEduArgsResponse;
  }

  /** @override */
  completeLogin(credentials, eduLoginParams) {
    this.methodCalled('completeLogin', [credentials, eduLoginParams]);
  }

  /** @override */
  getAccounts() {
    this.methodCalled('getAccounts');
    return Promise.resolve(getFakeAccountsList());
  }

  /** @override */
  consentLogged(account, eduCoexistenceTosVersion) {
    this.methodCalled('consentLogged', account, eduCoexistenceTosVersion);
    return this.consentLoggedResponse_();
  }

  /** @param {function} consentLoggedResponse */
  setConsentLoggedResponse(consentLoggedResponse) {
    this.consentLoggedResponse_ = consentLoggedResponse;
  }

  /** @override */
  dialogClose() {
    this.methodCalled('dialogClose');
  }

  /** @override */
  error() {
    this.methodCalled('error');
  }
}
