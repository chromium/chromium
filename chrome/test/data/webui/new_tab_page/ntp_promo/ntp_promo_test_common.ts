// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {NtpPromoProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import type {NtpPromoProxy} from 'chrome://new-tab-page/lazy_load.js';
import {NtpPromoClientCallbackRouter} from 'chrome://new-tab-page/new_tab_page.js';
import type {NtpPromoClientRemote, NtpPromoHandlerInterface} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestNtpPromoHandler extends TestBrowserProxy implements
    NtpPromoHandlerInterface {
  constructor() {
    super([
      'requestPromos',
      'onPromoShown',
      'onPromoClicked',
      'onPromoDismissed',
    ]);
  }

  requestPromos() {
    this.methodCalled('requestPromos');
  }

  onPromoShown(eligible: string) {
    this.methodCalled('onPromoShown', eligible);
  }

  onPromoClicked(promoId: string) {
    this.methodCalled('onPromoClicked', promoId);
  }

  onPromoDismissed(promoId: string) {
    this.methodCalled('onPromoDismissed', promoId);
  }
}

export class TestNtpPromoProxy implements NtpPromoProxy {
  private testHandler_ = new TestNtpPromoHandler();
  private callbackRouter_: NtpPromoClientCallbackRouter =
      new NtpPromoClientCallbackRouter();
  private callbackRouterRemote_: NtpPromoClientRemote;

  constructor() {
    this.callbackRouterRemote_ =
        this.callbackRouter_.$.bindNewPipeAndPassRemote();
  }

  static install(): TestNtpPromoProxy {
    const testProxy = new TestNtpPromoProxy();
    NtpPromoProxyImpl.setInstance(testProxy);
    return testProxy;
  }

  getHandler(): TestNtpPromoHandler {
    return this.testHandler_;
  }

  getCallbackRouter(): NtpPromoClientCallbackRouter {
    return this.callbackRouter_;
  }

  getCallbackRouterRemote(): NtpPromoClientRemote {
    return this.callbackRouterRemote_;
  }
}
