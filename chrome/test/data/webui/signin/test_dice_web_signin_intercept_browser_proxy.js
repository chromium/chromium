// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AccountInfo, DiceWebSigninInterceptBrowserProxy, InterceptionParameters} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';

import {TestBrowserProxy} from '../test_browser_proxy.m.js';

/** @implements {DiceWebSigninInterceptBrowserProxy} */
export class TestDiceWebSigninInterceptBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['accept', 'cancel', 'guest', 'pageLoaded']);
    /** @private {!InterceptionParameters} */
    this.interceptionParameters_ = {
      headerText: '',
      bodyTitle: '',
      bodyText: '',
      cancelButtonLabel: '',
      confirmButtonLabel: '',
      showGuestOption: false,
      headerTextColor: '',
      headerBackgroundColor: '',
      interceptedAccount: {isManaged: false, pictureUrl: ''},
    };
  }

  /** @param {!InterceptionParameters} parameters */
  setInterceptionParameters(parameters) {
    this.interceptionParameters_ = parameters;
  }

  /** @override */
  accept() {
    this.methodCalled('accept');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }

  /** @override */
  guest() {
    this.methodCalled('guest');
  }

  /** @override */
  pageLoaded() {
    this.methodCalled('pageLoaded');
    return Promise.resolve(this.interceptionParameters_);
  }
}
