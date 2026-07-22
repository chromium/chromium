// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of NotificationCardsProxy. */

import type {NotificationCard, NotificationCardsProxy} from 'chrome://password-manager/password_manager.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Test implementation
 */
export class TestNotificationCardsProxy extends TestBrowserProxy implements
    NotificationCardsProxy {
  card: NotificationCard|null;

  constructor() {
    super([
      'getAvailableNotificationCard',
      'recordNotificationDismissed',
    ]);

    this.card = null;
  }

  getAvailableNotificationCard() {
    this.methodCalled('getAvailableNotificationCard');
    return Promise.resolve(this.card);
  }

  recordNotificationDismissed(id: string) {
    this.methodCalled('recordNotificationDismissed', id);
  }
}
