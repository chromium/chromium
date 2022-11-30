// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DiceWebSigninInterceptBrowserProxy, InterceptionParameters} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDiceWebSigninInterceptBrowserProxy extends TestBrowserProxy
    implements DiceWebSigninInterceptBrowserProxy {
  private interceptionParameters_: InterceptionParameters;

  constructor() {
    super(['accept', 'cancel', 'guest', 'pageLoaded']);

    this.interceptionParameters_ = {
      headerText: '',
      bodyTitle: '',
      bodyText: '',
      cancelButtonLabel: '',
      confirmButtonLabel: '',
      managedDisclaimerText: '',
      showGuestOption: false,
      headerTextColor: '',
      interceptedProfileColor: '',
      primaryProfileColor: '',
      interceptedAccount: {isManaged: false, pictureUrl: ''},
      primaryAccount: {isManaged: false, pictureUrl: ''},
      useV2Design: false,
      showManagedDisclaimer: false,
    };
  }

  setInterceptionParameters(parameters: InterceptionParameters) {
    this.interceptionParameters_ = parameters;
  }

  accept() {
    this.methodCalled('accept');
  }

  cancel() {
    this.methodCalled('cancel');
  }

  guest() {
    this.methodCalled('guest');
  }

  pageLoaded() {
    this.methodCalled('pageLoaded');
    return Promise.resolve(this.interceptionParameters_);
  }
}
