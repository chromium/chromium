// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrivacyGuideBrowserProxy} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestPrivacyGuideBrowserProxy extends TestBrowserProxy implements
    PrivacyGuideBrowserProxy {
  private shouldShowAdTopicsCard_ = false;
  private privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel_ =
      false;

  constructor() {
    super([
      'getPromoImpressionCount',
      'incrementPromoImpressionCount',
      'privacySandboxPrivacyGuideShouldShowAdTopicsCard',
      'privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel',
    ]);
  }

  // Setters for test.
  setShouldShowAdTopicsCardForTesting(shouldShow: boolean) {
    this.shouldShowAdTopicsCard_ = shouldShow;
  }

  setPrivacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel(
      shouldShow: boolean) {
    this.privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel_ =
        shouldShow;
  }

  // Getters for test.
  getShouldShowAdTopicsCardForTesting(): boolean {
    return this.shouldShowAdTopicsCard_;
  }

  // Test Proxy Functions.
  getPromoImpressionCount() {
    this.methodCalled('getPromoImpressionCount');
    return 0;
  }

  incrementPromoImpressionCount() {
    this.methodCalled('incrementPromoImpressionCount');
  }

  privacySandboxPrivacyGuideShouldShowAdTopicsCard() {
    this.methodCalled('privacySandboxPrivacyGuideShouldShowAdTopicsCard');
    return Promise.resolve(this.shouldShowAdTopicsCard_);
  }

  privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel() {
    this.methodCalled(
        'privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel');
    return Promise.resolve(
        this.privacySandboxPrivacyGuideShouldShowCompletionCardAdTopicsSubLabel_);
  }
}
