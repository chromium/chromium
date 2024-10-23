// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UserAnnotationsManagerProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserAnnotationsManagerProxyImpl extends TestBrowserProxy
    implements UserAnnotationsManagerProxy {
  private entries_: chrome.autofillPrivate.UserAnnotationsEntry[] = [];
  private entriesBootstrapped_: boolean = false;
  private eligible_: boolean = true;

  constructor() {
    super([
      'getEntries',
      'deleteEntry',
      'deleteAllEntries',
      'triggerBootstrapping',
      'hasEntries',
      'isUserEligible',
      'predictionImprovementsIphFeatureUsed',
    ]);
  }

  setEntries(entries: chrome.autofillPrivate.UserAnnotationsEntry[]): void {
    this.entries_ = entries;
  }

  setEntriesBootstrapped(entriesBootstrapped: boolean): void {
    this.entriesBootstrapped_ = entriesBootstrapped;
  }

  setEligibility(isEligible: boolean): void {
    this.eligible_ = isEligible;
  }

  getEntries(): Promise<chrome.autofillPrivate.UserAnnotationsEntry[]> {
    this.methodCalled('getEntries');
    return Promise.resolve(this.entries_.slice());
  }

  deleteEntry(entryId: number): void {
    this.methodCalled('deleteEntry', entryId);
  }

  deleteAllEntries(): void {
    this.methodCalled('deleteAllEntries');
  }

  triggerBootstrapping(): Promise<boolean> {
    this.methodCalled('triggerBootstrapping');
    return Promise.resolve(this.entriesBootstrapped_);
  }

  hasEntries(): Promise<boolean> {
    this.methodCalled('hasEntries');
    return Promise.resolve(this.entries_.length > 0);
  }

  isUserEligible(): Promise<boolean> {
    this.methodCalled('isUserEligible');
    return Promise.resolve(this.eligible_);
  }

  predictionImprovementsIphFeatureUsed(): void {
    this.methodCalled('predictionImprovementsIphFeatureUsed');
  }
}
