// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FeaturePromoFeatureUsedAction, FeaturePromoParams, UserEducationBrowserProxy, UserEducationMixedTrustHandlerInterface} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestUserEducationBrowserProxy extends TestBrowserProxy implements
    UserEducationBrowserProxy {
  handler: UserEducationMixedTrustHandlerInterface;
  private showNewBadgeResponses_: Map<string, boolean> = new Map();

  constructor() {
    super([
      'notifyFeaturePromoFeatureUsed',
      'notifyAdditionalConditionEvent',
      'notifyNewBadgeFeatureUsed',
      'maybeShowNewBadgeFor',
    ]);
    this.handler = this;
  }

  maybeShowFeaturePromo(params: FeaturePromoParams) {
    this.methodCalled('maybeShowFeaturePromo', params);
  }

  notifyFeaturePromoFeatureUsed(
      featureName: string, action: FeaturePromoFeatureUsedAction) {
    this.methodCalled('notifyFeaturePromoFeatureUsed', {featureName, action});
  }

  notifyAdditionalConditionEvent(name: string) {
    this.methodCalled('notifyAdditionalConditionEvent', name);
  }

  notifyNewBadgeFeatureUsed(featureName: string) {
    this.methodCalled('notifyNewBadgeFeatureUsed', featureName);
  }

  setNewBadgeResponse(featureName: string, response: boolean) {
    this.showNewBadgeResponses_.set(featureName, response);
  }

  maybeShowNewBadgeFor(featureName: string): Promise<{shouldShow: boolean}> {
    this.methodCalled('maybeShowNewBadgeFor', featureName);
    return Promise.resolve(
        {shouldShow: this.showNewBadgeResponses_.get(featureName) || false});
  }
}
