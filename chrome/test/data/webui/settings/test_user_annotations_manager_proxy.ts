// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UserAnnotationsManagerProxy} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestUserAnnotationsManagerProxyImpl extends TestBrowserProxy
    implements UserAnnotationsManagerProxy {
  private entries: chrome.autofillPrivate.UserAnnotationsEntry[] = [];
  private eligible: boolean = true;

  constructor() {
    super([
      'getEntries',
      'deleteEntry',
      'deleteAllEntries',
      'hasEntries',
      'isUserEligible',
    ]);
  }

  setEntries(entries: chrome.autofillPrivate.UserAnnotationsEntry[]): void {
    this.entries = entries;
  }

  setEligibility(isEligible: boolean): void {
    this.eligible = isEligible;
  }

  getEntries(): Promise<chrome.autofillPrivate.UserAnnotationsEntry[]> {
    this.methodCalled('getEntries');
    return Promise.resolve(this.entries);
  }

  deleteEntry(entryId: number): void {
    this.methodCalled('deleteEntry', entryId);
  }

  deleteAllEntries(): void {
    this.methodCalled('deleteAllEntries');
  }

  hasEntries(): Promise<boolean> {
    this.methodCalled('hasEntries');
    return Promise.resolve(this.entries.length > 0);
  }

  isUserEligible(): Promise<boolean> {
    this.methodCalled('isUserEligible');
    return Promise.resolve(this.eligible);
  }
}
