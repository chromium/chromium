// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ChromeSigninInterceptionParameters, DiceWebSigninInterceptBrowserProxy, InterceptionParameters} from 'chrome://signin-dice-web-intercept/dice_web_signin_intercept_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDiceWebSigninInterceptBrowserProxy extends TestBrowserProxy
    implements DiceWebSigninInterceptBrowserProxy {
  private interceptionParameters_: InterceptionParameters;
  private chromeSigninInterceptionParameters_:
      ChromeSigninInterceptionParameters;

  constructor() {
    super([
      'accept',
      'cancel',
      'pageLoaded',
      'chromeSigninPageLoaded',
      'initializedWithHeight',
    ]);

    this.interceptionParameters_ = {
      headerText: '',
      bodyTitle: '',
      bodyText: '',
      cancelButtonLabel: '',
      confirmButtonLabel: '',
      managedDisclaimerText: '',
      headerTextColor: '',
      interceptedProfileColor: '',
      primaryProfileColor: '',
      interceptedAccount: {isManaged: false, pictureUrl: ''},
      primaryAccount: {isManaged: false, pictureUrl: ''},
      useV2Design: false,
      showManagedDisclaimer: false,
    };

    this.chromeSigninInterceptionParameters_ = {
      fullName: '',
      givenName: '',
      email: '',
      pictureUrl: '',
    };
  }

  setInterceptionParameters(parameters: InterceptionParameters) {
    this.interceptionParameters_ = parameters;
  }

  setChromeSigninInterceptionParameters(
      parameters: ChromeSigninInterceptionParameters) {
    this.chromeSigninInterceptionParameters_ = parameters;
  }

  accept() {
    this.methodCalled('accept');
  }

  cancel() {
    this.methodCalled('cancel');
  }

  pageLoaded() {
    this.methodCalled('pageLoaded');
    return Promise.resolve(this.interceptionParameters_);
  }

  chromeSigninPageLoaded(): Promise<ChromeSigninInterceptionParameters> {
    this.methodCalled('chromeSigninPageLoaded');
    return Promise.resolve(this.chromeSigninInterceptionParameters_);
  }

  initializedWithHeight(height: number) {
    this.methodCalled('initializedWithHeight', height);
  }
}
