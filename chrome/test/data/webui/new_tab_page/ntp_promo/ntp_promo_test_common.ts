// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import type {NtpPromoHandlerInterface} from 'chrome://new-tab-page/new_tab_page.js';
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
