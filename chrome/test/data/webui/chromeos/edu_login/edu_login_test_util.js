// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @param {string} email
 * @param {string} displayName
 * @param {string} profileImage
 * @param {string} obfuscatedGaiaId
 * @return {!ParentAccount}
 */
export function getFakeParent(
    email, displayName, profileImage, obfuscatedGaiaId) {
  return {
    email: email,
    displayName: displayName,
    profileImage: profileImage,
    obfuscatedGaiaId: obfuscatedGaiaId,
  };
}

/** @return {!Array<!ParentAccount>} */
export function getFakeParentsList() {
  return [
    getFakeParent('parent1@gmail.com', 'Parent 1', '', 'parent1gaia'),
    getFakeParent('parent2@gmail.com', 'Parent 2', '', 'parent2gaia'),
  ];
}

/** @return {!Array<string>} */
export function getFakeAccountsList() {
  return ['test@gmail.com', 'test2@gmail.com', 'test3@gmail.com'];
}

/** @implements {EduAccountLoginBrowserProxy} */
export class TestEduAccountLoginBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isNetworkReady',
      'getParents',
      'parentSignin',
      'loginInitialize',
      'authExtensionReady',
      'switchToFullTab',
      'completeLogin',
      'getAccounts',
      'dialogClose',
    ]);

    /** @private {function} */
    this.parentSigninResponse_ = null;
    /** @private {function} */
    this.getParentsResponse_ = null;
    /** @private {function} */
    this.isNetworkReadyResponse_ = null;
  }

  /** @override */
  isNetworkReady() {
    this.methodCalled('isNetworkReady');
    return this.isNetworkReadyResponse_ ? this.isNetworkReadyResponse_() :
                                          Promise.resolve(true);
  }

  /** @param {function} isNetworkReadyResponse */
  setIsNetworkReadyResponse(isNetworkReadyResponse) {
    this.isNetworkReadyResponse_ = isNetworkReadyResponse;
  }

  /** @override */
  getParents() {
    this.methodCalled('getParents');
    return this.getParentsResponse_ ? this.getParentsResponse_() :
                                      Promise.resolve(getFakeParentsList());
  }

  /** @param {function} getParentsResponse */
  setGetParentsResponse(getParentsResponse) {
    this.getParentsResponse_ = getParentsResponse;
  }

  /** @override */
  parentSignin(parent, password) {
    this.methodCalled('parentSignin', parent, password);
    return this.parentSigninResponse_();
  }

  /** @param {function} parentSigninResponse */
  setParentSigninResponse(parentSigninResponse) {
    this.parentSigninResponse_ = parentSigninResponse;
  }

  /** @override */
  loginInitialize() {
    this.methodCalled('loginInitialize');
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
  completeLogin(credentials, eduLoginParams) {
    this.methodCalled('completeLogin', [credentials, eduLoginParams]);
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
