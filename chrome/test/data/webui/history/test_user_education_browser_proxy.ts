// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy, FeaturePromoFeatureUsedAction, FeaturePromoParams, UserEducationMixedTrustHandlerInterface} from 'chrome://resources/mojo/components/user_education/webui/user_education.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

// TODO(https://crbug.com/546619486): Currently this code is duplicated here and
// in read_anything with minor differences. Find a way to make this a shared
// file.

export class TestUserEducationBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  handler: UserEducationMixedTrustHandlerInterface;
  private showNewBadgeResponses_: Map<string, boolean> = new Map();

  constructor() {
    super([
      'notifyFeaturePromoFeatureUsed',
      'notifyAdditionalConditionEvent',
      'notifyNewBadgeFeatureUsed',
      'maybeShowFeaturePromo',
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
