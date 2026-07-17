// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PromoCardsProxy. */

import type {PromoCard, PromoCardsProxy} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test implementation
 */
export class TestPromoCardsProxy extends TestBrowserProxy implements
    PromoCardsProxy {
  promo: PromoCard|null;

  constructor() {
    super([
      'getAvailablePromoCard',
      'recordPromoDismissed',
    ]);

    this.promo = null;
  }

  getAvailablePromoCard() {
    this.methodCalled('getAvailablePromoCard');
    return Promise.resolve(this.promo);
  }

  recordPromoDismissed(id: string) {
    this.methodCalled('recordPromoDismissed', id);
  }
}
